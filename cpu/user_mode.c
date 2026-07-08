#include "user_mode.h"
#include "../drivers/keyboard.h"
#include "../drivers/screen.h"
#include "../kernel/string.h"
#include "../memory/paging.h"
#include "../task/task.h"
#include "tss.h"
#include "user_test.h"

static void prepare_user_program(user_program_t *program) {
  for (u32 i = 0; i < program->region_count; i++) {
    mark_user_range(program->regions[i].start, program->regions[i].len);
  }
}

#define USER_KERNEL_STACK_SIZE 4096
static user_process_t user_process_table[MAX_USER_PROCESSES];

static u8 user_kernel_stack[USER_KERNEL_STACK_SIZE];
static user_process_t *current_user_process;

static user_region_t user_test_regions[7];
static user_program_t user_test_program;
static user_region_t user_fault_regions[3];
static user_program_t user_fault_program;
static int builtin_user_programs_ready;

static void init_user_fault_process(user_process_t *process);

int user_memory_ok(u32 ptr, u32 len, u32 required_flags) {
  if (!current_user_process || !current_user_process->program)
    return 0;

  if (!user_pages_ok(ptr, len))
    return 0;

  if (len == 0)
    return 1;

  u32 end = ptr + len - 1;
  if (end < ptr)
    return 0;

  user_program_t *program = current_user_process->program;

  for (u32 i = 0; i < program->region_count; i++) {
    user_region_t *region = &program->regions[i];
    u32 region_end = region->start + region->len - 1;

    if (region->len == 0 || region_end < region->start)
      continue;

    if (ptr >= region->start && end <= region_end &&
        (region->flags & required_flags) == required_flags) {
      return 1;
    }
  }

  return 0;
}

static void user_program_set(user_program_t *program, u32 entry, u32 stack_top,
                             user_region_t *regions, u32 region_count) {
  program->entry = entry;
  program->stack_top = stack_top;
  program->regions = regions;
  program->region_count = region_count;
}

static void user_region_set(user_region_t *region, u32 start, u32 len,
                            u32 flags) {
  region->start = start;
  region->len = len;
  region->flags = flags;
}

static void build_builtin_user_test_program(void) {
  user_region_set(&user_test_regions[0], (u32)user_test_main, FRAME_SIZE,
                  USER_REGION_READ | USER_REGION_EXEC);
  user_region_set(&user_test_regions[1], (u32)user_test_msg, user_test_msg_len,
                  USER_REGION_READ);
  user_region_set(&user_test_regions[2], (u32)user_test_bad_ptr_msg,
                  user_test_bad_ptr_msg_len, USER_REGION_READ);
  user_region_set(&user_test_regions[3], (u32)user_test_input_buf,
                  user_test_input_buf_len,
                  USER_REGION_READ | USER_REGION_WRITE);
  user_region_set(&user_test_regions[4], (u32)user_test_prompt,
                  user_test_prompt_len, USER_REGION_READ);
  user_region_set(&user_test_regions[5], (u32)user_test_got_msg,
                  user_test_got_msg_len, USER_REGION_READ);
  user_region_set(&user_test_regions[6], USER_STACK_TOP - FRAME_SIZE,
                  FRAME_SIZE, USER_REGION_READ | USER_REGION_WRITE);

  user_program_set(&user_test_program, (u32)user_test_main, USER_STACK_TOP,
                   user_test_regions,
                   sizeof(user_test_regions) / sizeof(user_test_regions[0]));
}

static void build_builtin_user_fault_program(void) {
  user_region_set(&user_fault_regions[0], (u32)user_fault_test_main, FRAME_SIZE,
                  USER_REGION_READ | USER_REGION_EXEC);
  user_region_set(&user_fault_regions[1], (u32)user_fault_test_msg,
                  user_fault_test_len, USER_REGION_READ);
  user_region_set(&user_fault_regions[2], USER_STACK_TOP - FRAME_SIZE,
                  FRAME_SIZE, USER_REGION_READ | USER_REGION_WRITE);

  user_program_set(&user_fault_program, (u32)user_fault_test_main,
                   USER_STACK_TOP, user_fault_regions,
                   sizeof(user_fault_regions) / sizeof(user_fault_regions[0]));
}

static void init_builtin_user_programs(void) {
  if (builtin_user_programs_ready)
    return;

  build_builtin_user_test_program();
  build_builtin_user_fault_program();

  builtin_user_programs_ready = 1;
}

static void print_user_process_status(user_process_t *process) {
  char buff[16];

  print(process->name);
  if (process->state == USER_PROCESS_FAILED)
    print(" failed: ");
  else
    print(" exited: ");

  int_to_ascii(process->exit_code, buff);
  println(buff);
}

static user_process_t *user_process_alloc() {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    if (user_process_table[i].state == USER_PROCESS_UNUSED) {
      user_process_table[i].state = USER_PROCESS_READY;
      return &user_process_table[i];
    }
  }
  return 0;
}

int run_user_program(user_program_t *program) {
  u32 ret = user_context_save();

  if (ret != 0) {
    keyboard_set_owner(KEYBOARD_OWNER_SHELL);
    tss_set_kernel_stack(0x90000);
    __asm__ __volatile__("sti");
    return ret - 1;
  }
  keyboard_clear_line();
  keyboard_set_owner(KEYBOARD_OWNER_USER);
  prepare_user_program(program);
  tss_set_kernel_stack((u32)user_kernel_stack + USER_KERNEL_STACK_SIZE);
  enter_user_mode(program->entry, program->stack_top);

  tss_set_kernel_stack(0x90000);
  return -1;
}

static const char *user_process_state_name(user_process_state_t state) {
  if (state == USER_PROCESS_UNUSED)
    return "UNUSED";
  if (state == USER_PROCESS_READY)
    return "READY";
  if (state == USER_PROCESS_RUNNING)
    return "RUNNING";
  if (state == USER_PROCESS_EXITED)
    return "EXITED";
  if (state == USER_PROCESS_FAILED)
    return "FAILED";
  return "?";
}

void user_exit_current(u32 code) { user_context_restore(code + 1); }

int run_user_process(user_process_t *process) {
  current_user_process = process;
  process->state = USER_PROCESS_RUNNING;

  int code = run_user_program(process->program);

  if ((u32)code == USER_EXIT_FAULT) {
    process->state = USER_PROCESS_FAILED;
  } else if (code < 0) {
    process->state = USER_PROCESS_FAILED;
  } else {
    process->state = USER_PROCESS_EXITED;
  }

  process->exit_code = code;
  current_user_process = 0;

  return code;
}

static void init_user_test_process(user_process_t *process) {
  init_builtin_user_programs();

  *process = (user_process_t){
      .name = "user_test",
      .program = &user_test_program,
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
}

int run_user_test() {
  user_process_t process;
  init_user_test_process(&process);

  return run_user_process(&process);
}

void user_test_task_entry() {
  user_process_t *process = user_process_alloc();
  if (!process) {
    println("no process slots");
    task_exit();
  }

  init_user_test_process(process);
  run_user_process(process);
  print_user_process_status(process);
  task_exit();
}

void user_fault_task_entry(void) {
  user_process_t *process = user_process_alloc();
  if (!process) {
    println("no process slots");
    task_exit();
  }

  init_user_fault_process(process);
  run_user_process(process);
  print_user_process_status(process);
  task_exit();
}

static void init_user_fault_process(user_process_t *process) {
  init_builtin_user_programs();

  *process = (user_process_t){
      .name = "user_fault",
      .program = &user_fault_program,
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
}

int run_user_fault_test(void) {
  user_process_t process;
  init_user_fault_process(&process);

  return run_user_process(&process);
}

void user_fault_current(void) {
  if (current_user_process)
    current_user_process->state = USER_PROCESS_FAILED;
  user_context_restore(USER_EXIT_FAULT + 1);
}

void user_process_dump(void) {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    user_process_t *p = &user_process_table[i];

    if (p->state == USER_PROCESS_UNUSED)
      continue;

    char buf[16];

    print(p->name);
    print(" ");

    print(user_process_state_name(p->state));
    print(" ");

    int_to_ascii(p->exit_code, buf);
    println(buf);
  }
}

void user_process_reap(void) {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    user_process_t *p = &user_process_table[i];

    if (p == current_user_process)
      continue;

    if (p->state == USER_PROCESS_EXITED || p->state == USER_PROCESS_FAILED) {
      p->state = USER_PROCESS_UNUSED;
      p->name = 0;
      p->program = 0;
      p->exit_code = 0;
    }
  }
}
