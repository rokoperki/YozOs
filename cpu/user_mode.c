#include "user_mode.h"
#include "../drivers/keyboard.h"
#include "../drivers/screen.h"
#include "../fs/fat.h"
#include "../kernel/mem.h"
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
#define USER_PROCESS_NAME_LEN 16
static u32 next_user_pid = 1;
static user_process_t user_process_table[MAX_USER_PROCESSES];
static char user_process_names[MAX_USER_PROCESSES][USER_PROCESS_NAME_LEN];

// Process lifecycle:
// UNUSED -> READY -> RUNNING -> EXITED/FAILED/KILLED -> reaped to UNUSED.
//
// task_t owns scheduler execution state. user_process_t owns process identity,
// parent metadata, status, and exit accounting. For now, user processes and
// tasks are separate but linked one-to-one while the process is active.

static u8 user_kernel_stack[USER_KERNEL_STACK_SIZE];
static user_process_t *current_user_process;

static user_region_t user_test_regions[7];
static user_program_t user_test_program;
static user_region_t user_fault_regions[3];
static user_program_t user_fault_program;
static int builtin_user_programs_ready;
static user_process_t *pending_user_test_process;
static user_process_t *pending_user_fault_process;

// Single-slot external loader: one FAT-loaded program may be prepared/running
// at a time because all external binaries load at USER_LOAD_ADDR.
static user_region_t loaded_user_regions[2];
static user_program_t loaded_user_program;
static user_process_t *loaded_user_process;
static int loaded_user_busy;

static void init_user_fault_process(user_process_t *process);
static void user_file_task_entry(void);

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

static int user_process_is_dead(user_process_t *process) {
  return process->state == USER_PROCESS_EXITED ||
         process->state == USER_PROCESS_FAILED ||
         process->state == USER_PROCESS_KILLED;
}

static void user_process_mark_exited(user_process_t *process, u32 code) {
  process->state = USER_PROCESS_EXITED;
  process->exit_code = code;
}

static void user_process_mark_failed(user_process_t *process, u32 code) {
  process->state = USER_PROCESS_FAILED;
  process->exit_code = code;
}

static void user_process_mark_killed(user_process_t *process) {
  process->state = USER_PROCESS_KILLED;
  process->exit_code = USER_EXIT_KILLED;
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

static u32 read_u32(u8 *ptr) {
  return ((u32)ptr[0]) | ((u32)ptr[1] << 8) | ((u32)ptr[2] << 16) |
         ((u32)ptr[3] << 24);
}

static int user_binary_header_ok(u8 *image, u32 loaded_len, u32 *entry) {
  if (loaded_len < USER_BIN_HEADER_SIZE) {
    println("bad user binary");
    return 0;
  }

  u32 magic = read_u32(image);
  u32 load_addr = read_u32(image + 4);
  u32 entry_offset = read_u32(image + 8);
  u32 image_size = read_u32(image + 12);

  if (magic != USER_BIN_MAGIC) {
    println("bad user binary");
    return 0;
  }

  if (image_size < USER_BIN_HEADER_SIZE || image_size > USER_LOAD_MAX_BYTES) {
    println("bad user binary");
    return 0;
  }

  if (load_addr != USER_LOAD_ADDR || image_size != loaded_len ||
      entry_offset < USER_BIN_HEADER_SIZE || entry_offset >= loaded_len) {
    println("bad user binary");
    return 0;
  }

  *entry = USER_LOAD_ADDR + entry_offset;
  return 1;
}

static void build_loaded_user_program(u32 loaded_len, u32 entry) {
  user_region_set(&loaded_user_regions[0], USER_LOAD_ADDR, loaded_len,
                  USER_REGION_READ | USER_REGION_WRITE | USER_REGION_EXEC);

  user_region_set(&loaded_user_regions[1], USER_STACK_TOP - FRAME_SIZE,
                  FRAME_SIZE, USER_REGION_READ | USER_REGION_WRITE);

  user_program_set(
      &loaded_user_program, entry, USER_STACK_TOP, loaded_user_regions,
      sizeof(loaded_user_regions) / sizeof(loaded_user_regions[0]));
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

static user_process_t *user_process_find_pid(u32 pid) {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    user_process_t *p = &user_process_table[i];

    if (p->state == USER_PROCESS_UNUSED)
      continue;

    if (p->pid == pid)
      return p;
  }

  return 0;
}

static void user_process_assign_pid(user_process_t *process) {
  if (process->pid == 0)
    process->pid = next_user_pid++;
}

static void user_process_clear(user_process_t *process) {
  process->state = USER_PROCESS_UNUSED;
  process->name = 0;
  process->pid = 0;
  process->parent_pid = 0;
  process->task = 0;
  process->program = 0;
  process->exit_code = 0;

  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    if (&user_process_table[i] == process) {
      user_process_names[i][0] = '\0';
      return;
    }
  }
}

static void user_process_set_name(user_process_t *process, char *name) {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    if (&user_process_table[i] != process)
      continue;

    int j = 0;
    while (j + 1 < USER_PROCESS_NAME_LEN && name[j]) {
      user_process_names[i][j] = name[j];
      j++;
    }
    user_process_names[i][j] = '\0';
    process->name = user_process_names[i];
    return;
  }

  process->name = name;
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
  if (state == USER_PROCESS_KILLED)
    return "KILLED";
  return "?";
}

void user_exit_current(u32 code) { user_context_restore(code + 1); }

int run_user_process(user_process_t *process) {
  current_user_process = process;
  process->state = USER_PROCESS_RUNNING;

  int code = run_user_program(process->program);

  if ((u32)code == USER_EXIT_FAULT) {
    user_process_mark_failed(process, USER_EXIT_FAULT);
  } else {
    user_process_mark_exited(process, (u32)code);
  }

  current_user_process = 0;

  return code;
}

static void init_user_test_process(user_process_t *process) {
  init_builtin_user_programs();

  *process = (user_process_t){
      .name = "user_test",
      .pid = 0,
      .parent_pid = 0,
      .task = 0,
      .program = &user_test_program,
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
  user_process_assign_pid(process);
}

int run_user_test() {
  user_process_t process;
  init_user_test_process(&process);

  return run_user_process(&process);
}

void user_test_task_entry() {
  user_process_t *process = pending_user_test_process;
  pending_user_test_process = 0;

  if (!process) {
    println("no user process");
    task_exit();
  }

  run_user_process(process);
  print_user_process_status(process);
  task_exit();
}

void user_fault_task_entry(void) {
  user_process_t *process = pending_user_fault_process;
  pending_user_fault_process = 0;

  if (!process) {
    println("no user process");
    task_exit();
  }

  run_user_process(process);
  print_user_process_status(process);
  task_exit();
}

static void init_user_fault_process(user_process_t *process) {
  init_builtin_user_programs();

  *process = (user_process_t){
      .name = "user_fault",
      .pid = 0,
      .task = 0,
      .program = &user_fault_program,
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
  user_process_assign_pid(process);
}

int run_user_fault_test(void) {
  user_process_t process;
  init_user_fault_process(&process);

  return run_user_process(&process);
}

task_t *start_user_test_task(void) {
  if (pending_user_test_process) {
    println("user test busy");
    return 0;
  }

  user_process_t *process = user_process_alloc();
  if (!process) {
    println("no process slots");
    return 0;
  }

  init_user_test_process(process);
  pending_user_test_process = process;

  task_t *task = spawn_task(process->name, user_test_task_entry);
  if (!task) {
    println("no task slot");
    pending_user_test_process = 0;
    user_process_clear(process);
    return 0;
  }

  process->task = task;
  return task;
}

task_t *start_user_fault_task(void) {
  if (pending_user_fault_process) {
    println("user fault busy");
    return 0;
  }

  user_process_t *process = user_process_alloc();
  if (!process) {
    println("no process slots");
    return 0;
  }

  init_user_fault_process(process);
  pending_user_fault_process = process;

  task_t *task = spawn_task(process->name, user_fault_task_entry);
  if (!task) {
    println("no task slot");
    pending_user_fault_process = 0;
    user_process_clear(process);
    return 0;
  }

  process->task = task;
  return task;
}

void user_fault_current(void) {
  if (current_user_process)
    user_process_mark_failed(current_user_process, USER_EXIT_FAULT);
  user_context_restore(USER_EXIT_FAULT + 1);
}

void user_process_dump(void) {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    user_process_t *p = &user_process_table[i];

    if (p->state == USER_PROCESS_UNUSED)
      continue;

    char buf[16];

    int_to_ascii(p->pid, buf);
    print(buf);
    print(" ");

    print(" ppid=");
    int_to_ascii(p->parent_pid, buf);
    print(buf);
    print(" ");

    print(p->name);
    print(" ");

    print(user_process_state_name(p->state));
    print(" ");

    if (p->task) {
      print(" task=");
      int_to_ascii(task_get_id(p->task), buf);
      print(buf);
    }

    print(" exit_code= ");
    int_to_ascii(p->exit_code, buf);
    println(buf);
  }
}

void user_process_reap(void) {
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    user_process_t *p = &user_process_table[i];

    if (p == current_user_process)
      continue;

    if (user_process_is_dead(p)) {
      user_process_clear(p);
    }
  }
}

int user_process_reap_pid(u32 pid) {
  user_process_t *p = user_process_find_pid(pid);

  if (!p) {
    println("no such process");
    return USER_REAP_NOT_FOUND;
  }

  if (p == current_user_process) {
    println("process is still running");
    return USER_REAP_RUNNING;
  }

  if (user_process_is_dead(p)) {
    user_process_clear(p);
    return USER_REAP_OK;
  }

  println("process is still running");
  return USER_REAP_RUNNING;
}

int user_process_kill_pid(u32 pid) {
  user_process_t *p = user_process_find_pid(pid);

  if (!p) {
    println("no such process");
    return USER_KILL_NOT_FOUND;
  }

  if (user_process_is_dead(p)) {
    println("process already dead");
    return USER_KILL_DEAD;
  }

  if (p->task == 0) {
    println("process has no task");
    return USER_KILL_NO_TASK;
  }

  if (task_kill(p->task) == 0) {
    println("could not kill task");
    return USER_KILL_NO_TASK;
  }

  if (p == loaded_user_process) {
    loaded_user_process = 0;
    loaded_user_busy = 0;
  }

  user_process_mark_killed(p);
  println("process killed");
  return USER_KILL_OK;
}

int user_process_wait_pid(u32 pid) {
  user_process_t *p = user_process_find_pid(pid);

  if (!p) {
    println("no such process");
    return USER_WAIT_NOT_FOUND;
  }

  char buf[16];

  if (p->state == USER_PROCESS_EXITED) {
    print("process exited: ");
    int_to_ascii(p->exit_code, buf);
    println(buf);
    return USER_WAIT_OK;
  }

  if (p->state == USER_PROCESS_FAILED) {
    print("process failed: ");
    int_to_ascii(p->exit_code, buf);
    println(buf);
    return USER_WAIT_OK;
  }

  if (p->state == USER_PROCESS_KILLED) {
    print("process killed: ");
    int_to_ascii(p->exit_code, buf);
    println(buf);
    return USER_WAIT_OK;
  }

  println("process still running");
  return USER_WAIT_RUNNING;
}

u32 user_waitpid_status(u32 pid) {
  user_process_t *p = user_process_find_pid(pid);

  if (!p)
    return USER_WAITPID_NOT_FOUND;

  if (user_process_is_dead(p)) {
    return p->exit_code;
  }

  return USER_WAITPID_RUNNING;
}

u32 user_current_pid() {
  if (current_user_process == 0)
    return 0;

  return current_user_process->pid;
}

u32 user_current_ppid() {
  if (current_user_process == 0)
    return 0;

  return current_user_process->parent_pid;
}

static void user_file_task_entry(void) {
  if (!loaded_user_process) {
    loaded_user_busy = 0;
    task_exit();
  }

  run_user_process(loaded_user_process);
  print_user_process_status(loaded_user_process);

  loaded_user_process = 0;
  loaded_user_busy = 0;
  task_exit();
}

int run_user_file(char *name) {
  u32 loaded_len;
  u32 entry;

  if (loaded_user_busy) {
    println("user loader busy");
    return -1;
  }

  memory_set((u8 *)USER_LOAD_ADDR, 0, USER_LOAD_MAX_BYTES);
  if (!fat_read_file(name, (u8 *)USER_LOAD_ADDR, USER_LOAD_MAX_BYTES,
                     &loaded_len)) {
    return -1;
  }

  if (loaded_len == 0) {
    println("empty program");
    return -1;
  }

  if (!user_binary_header_ok((u8 *)USER_LOAD_ADDR, loaded_len, &entry))
    return -1;

  build_loaded_user_program(loaded_len, entry);

  user_process_t *process = user_process_alloc();
  if (!process) {
    println("no process slots");
    return -1;
  }

  *process = (user_process_t){
      .name = 0,
      .pid = 0,
      .parent_pid = 0,
      .task = 0,
      .program = &loaded_user_program,
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
  user_process_assign_pid(process);
  user_process_set_name(process, name);

  loaded_user_process = process;
  loaded_user_busy = 1;

  task_t *task = spawn_task(process->name, user_file_task_entry);
  if (!task) {
    println("no task slot");
    loaded_user_process = 0;
    loaded_user_busy = 0;
    process->state = USER_PROCESS_UNUSED;
    process->name = 0;
    process->pid = 0;
    process->task = 0;
    process->program = 0;
    process->exit_code = 0;
    return -1;
  }

  process->task = task;

  return 0;
}
