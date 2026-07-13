#include "user_mode.h"
#include "../drivers/keyboard.h"
#include "../drivers/screen.h"
#include "../fs/fat.h"
#include "../fs/vfs.h"
#include "../kernel/mem.h"
#include "../kernel/string.h"
#include "../memory/frame_alloc.h"
#include "../memory/paging.h"
#include "../task/task.h"
#include "tss.h"
#include "user_test.h"

static void prepare_user_program(address_space_t *space,
                                 user_program_t *program) {
  for (u32 i = 0; i < program->region_count; i++) {
    address_space_mark_user_range(space, program->regions[i].start,
                                  program->regions[i].len);
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

// External binaries are staged at USER_LOAD_ADDR, then copied into private
// process image frames before the task is started.

static void init_user_fault_process(user_process_t *process);
static void user_process_close_fds(user_process_t *process);
static void user_file_task_entry(void);

int user_memory_ok(u32 ptr, u32 len, u32 required_flags) {
  if (!current_user_process || !current_user_process->program ||
      !current_user_process->address_space)
    return 0;

  if (!address_space_user_pages_ok(current_user_process->address_space, ptr,
                                   len))
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

// Built-in user tests are kernel-backed ring-3 fixtures. Their code/data are
// linked into the kernel image and described as user regions for syscall/fault
// testing. Real loaded programs use private image frames; moving these fixtures
// to private mappings belongs with relocatable/ELF loading, not Phase 3.
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

static void build_loaded_user_program(user_process_t *process, u32 loaded_len,
                                      u32 entry) {
  user_region_set(&process->loaded_regions[0], USER_LOAD_ADDR, loaded_len,
                  USER_REGION_READ | USER_REGION_WRITE | USER_REGION_EXEC);

  user_region_set(&process->loaded_regions[1], USER_STACK_TOP - FRAME_SIZE,
                  FRAME_SIZE, USER_REGION_READ | USER_REGION_WRITE);

  user_program_set(
      &process->loaded_program, entry, USER_STACK_TOP, process->loaded_regions,
      sizeof(process->loaded_regions) / sizeof(process->loaded_regions[0]));
  process->program = &process->loaded_program;
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

static int user_process_map_stack(user_process_t *process) {
  u32 frame = alloc_frame();
  if (frame == 0)
    return 0;

  memory_set((u8 *)frame, 0, FRAME_SIZE);

  if (!address_space_map_page(process->address_space,
                              USER_STACK_TOP - FRAME_SIZE, frame,
                              PAGE_RW | PAGE_USER)) {
    free_frame(frame);
    return 0;
  }

  process->user_stack_frame = frame;
  return 1;
}

static int user_process_map_loaded_image(user_process_t *process, u32 image,
                                         u32 len) {
  u32 pages = (len + FRAME_SIZE - 1) / FRAME_SIZE;
  if (pages == 0 || pages > USER_MAX_IMAGE_FRAMES)
    return 0;

  for (u32 i = 0; i < pages; i++) {
    u32 frame = alloc_frame();
    if (frame == 0)
      return 0;

    memory_set((u8 *)frame, 0, FRAME_SIZE);

    u32 remaining = len - (i * FRAME_SIZE);
    u32 copy_len = remaining > FRAME_SIZE ? FRAME_SIZE : remaining;

    memory_copy((char *)(image + (i * FRAME_SIZE)), (char *)frame, copy_len);

    if (!address_space_map_page(process->address_space,
                                USER_LOAD_ADDR + (i * FRAME_SIZE), frame,
                                PAGE_RW | PAGE_USER)) {
      free_frame(frame);
      return 0;
    }

    process->image_frames[i] = frame;
    process->image_frame_count++;
  }

  return 1;
}

static void user_process_clear(user_process_t *process) {
  process->state = USER_PROCESS_UNUSED;
  process->name = 0;
  process->pid = 0;
  process->parent_pid = 0;
  process->task = 0;
  process->program = 0;

  if (process->user_stack_frame) {
    free_frame(process->user_stack_frame);
    process->user_stack_frame = 0;
  }

  for (u32 i = 0; i < process->image_frame_count; i++) {
    if (process->image_frames[i]) {
      free_frame(process->image_frames[i]);
    }
    process->image_frame_count = 0;
  }

  user_process_close_fds(process);

  address_space_destroy(process->address_space);
  process->address_space = 0;
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

static void user_process_init_fds(user_process_t *process) {
  for (int i = 0; i < USER_MAX_FDS; i++) {
    process->fds[i].type = USER_FD_TYPE_UNUSED;
    process->fds[i].vfs_handle = -1;
  }

  process->fds[USER_FD_STDIN].type = USER_FD_TYPE_STDIN;
  process->fds[USER_FD_STDOUT].type = USER_FD_TYPE_STDOUT;
  process->fds[USER_FD_STDERR].type = USER_FD_TYPE_STDERR;
}

static int user_fd_find_free(user_process_t *process) {
  for (int i = USER_FD_FIRST_FILE; i < USER_MAX_FDS; i++) {
    if (process->fds[i].type == USER_FD_TYPE_UNUSED)
      return i;
  }

  return -1;
}

static void user_process_close_fds(user_process_t *process) {
  for (int i = USER_FD_FIRST_FILE; i < USER_MAX_FDS; i++) {
    if (process->fds[i].type == USER_FD_TYPE_VFS) {
      vfs_close(process->fds[i].vfs_handle);
    }

    process->fds[i].type = USER_FD_TYPE_UNUSED;
    process->fds[i].vfs_handle = -1;
  }
}

int run_user_program(address_space_t *space, user_program_t *program) {
  u32 ret = user_context_save();

  if (ret != 0) {
    keyboard_set_owner(KEYBOARD_OWNER_SHELL);
    tss_set_kernel_stack(0x90000);
    __asm__ __volatile__("sti");
    return ret - 1;
  }
  keyboard_clear_line();
  keyboard_set_owner(KEYBOARD_OWNER_USER);
  prepare_user_program(space, program);
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
  if (!process->address_space) {
    user_process_mark_failed(process, 1);
    return 1;
  }

  current_user_process = process;
  process->state = USER_PROCESS_RUNNING;

  int code = run_user_program(process->address_space, process->program);

  if ((u32)code == USER_EXIT_FAULT) {
    user_process_mark_failed(process, USER_EXIT_FAULT);
  } else {
    user_process_mark_exited(process, (u32)code);
  }

  user_process_close_fds(process);
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
      .address_space = address_space_create_user(),
      .program = &user_test_program,
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
  user_process_init_fds(process);
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
      .address_space = address_space_create_user(),
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
  user_process_init_fds(process);
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
  if (!process->address_space) {
    println("no address space");
    user_process_clear(process);
    return 0;
  }
  if (!user_process_map_stack(process)) {
    println("no user stack");
    user_process_clear(process);
    return 0;
  }

  pending_user_test_process = process;

  task_t *task = spawn_task(process->name, user_test_task_entry);
  if (!task) {
    println("no task slot");
    pending_user_test_process = 0;
    user_process_clear(process);
    return 0;
  }

  process->task = task;
  task_set_user_process(task, process);
  task_set_address_space(task, process->address_space);
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
  if (!process->address_space) {
    println("no address space");
    user_process_clear(process);
    return 0;
  }
  if (!user_process_map_stack(process)) {
    println("no user stack");
    user_process_clear(process);
    return 0;
  }

  pending_user_fault_process = process;

  task_t *task = spawn_task(process->name, user_fault_task_entry);
  if (!task) {
    println("no task slot");
    pending_user_fault_process = 0;
    user_process_clear(process);
    return 0;
  }

  process->task = task;
  task_set_user_process(task, process);
  task_set_address_space(task, process->address_space);
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

    print(" pd=");
    int_to_ascii((u32)address_space_page_directory(p->address_space), buf);
    print(buf);

    print(" stack=");
    int_to_ascii(p->user_stack_frame, buf);
    print(buf);

    print(" img=");
    int_to_ascii(p->image_frame_count, buf);
    print(buf);

    for (u32 j = 0; j < p->image_frame_count; j++) {
      print(" imgf");
      int_to_ascii(j, buf);
      print(buf);
      print("=");
      int_to_ascii(p->image_frames[j], buf);
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

  user_process_mark_killed(p);
  user_process_close_fds(p);
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

u32 user_process_pid(struct user_process *process) {
  return process ? process->pid : 0;
}

u32 user_current_ppid() {
  if (current_user_process == 0)
    return 0;

  return current_user_process->parent_pid;
}

static void user_file_task_entry(void) {
  task_t *task = task_current();
  user_process_t *process = (user_process_t *)task_get_user_process(task);

  if (!process)
    task_exit();

  run_user_process(process);
  print_user_process_status(process);
  task_exit();
}

int run_user_file(char *name) {
  u32 loaded_len;
  u32 entry;

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
      .address_space = address_space_create_user(),
      .state = USER_PROCESS_READY,
      .exit_code = 0,
  };
  build_loaded_user_program(process, loaded_len, entry);

  user_process_init_fds(process);
  user_process_assign_pid(process);
  user_process_set_name(process, name);

  if (!process->address_space) {
    println("no address space");
    user_process_clear(process);
    return -1;
  }
  if (!user_process_map_stack(process)) {
    println("no user stack");
    user_process_clear(process);
    return -1;
  }

  if (!user_process_map_loaded_image(process, USER_LOAD_ADDR, loaded_len)) {
    print("no user image");
    user_process_clear(process);
    return -1;
  }

  task_t *task = spawn_task(process->name, user_file_task_entry);
  if (!task) {
    println("no task slot");
    user_process_clear(process);
    return -1;
  }

  process->task = task;
  task_set_user_process(task, process);
  task_set_address_space(task, process->address_space);

  return 0;
}

int user_fd_open_current(char *path, u32 flags) {
  if (!current_user_process)
    return -1;

  int fd = user_fd_find_free(current_user_process);
  if (fd < 0)
    return -1;

  int handle = vfs_open(path, flags);
  if (handle < 0)
    return handle;

  current_user_process->fds[fd].type = USER_FD_TYPE_VFS;
  current_user_process->fds[fd].vfs_handle = handle;

  return fd;
}

int user_fd_read_current(int fd, u8 *dst, u32 len) {
  if (!current_user_process)
    return -1;

  if (fd < 0 || fd >= USER_MAX_FDS)
    return -1;

  if (!dst && len > 0)
    return -1;

  user_fd_t *entry = &current_user_process->fds[fd];

  if (entry->type == USER_FD_TYPE_STDIN) {
    if (len == 0)
      return 0;

    if (!keyboard_line_ready())
      return 0;

    char *src = keyboard_get_line();

    u32 i = 0;
    while (i < len && src[i]) {
      dst[i] = src[i];
      i++;
    }

    keyboard_clear_line();
    return i;
  }

  if (entry->type == USER_FD_TYPE_VFS)
    return vfs_read(entry->vfs_handle, dst, len);

  return -1;
}

int user_fd_write_current(int fd, u8 *src, u32 len) {
  if (!current_user_process)
    return -1;

  if (fd < 0 || fd >= USER_MAX_FDS)
    return -1;

  user_fd_t *entry = &current_user_process->fds[fd];

  if (entry->type == USER_FD_TYPE_STDOUT ||
      entry->type == USER_FD_TYPE_STDERR) {
    for (u32 i = 0; i < len; i++) {
      print_char(src[i], -1, -1, WHITE_ON_BLACK);
    }

    return len;
  }

  return -1;
}

int user_fd_close_current(int fd) {
  if (!current_user_process)
    return -1;

  if (fd < 0 || fd >= USER_MAX_FDS)
    return -1;

  user_fd_t *entry = &current_user_process->fds[fd];

  if (entry->type != USER_FD_TYPE_VFS)
    return -1;

  int ret = vfs_close(entry->vfs_handle);

  entry->type = USER_FD_TYPE_UNUSED;
  entry->vfs_handle = -1;

  return ret;
}

int user_stat_path(char *path, user_stat_t *out) {
  if (!current_user_process)
    return -1;

  if (!out)
    return -1;

  vfs_stat_t st;
  int ret = vfs_stat(path, &st);
  if (ret < 0)
    return ret;

  out->size = st.size;
  out->type = USER_STAT_TYPE_FILE;
  return 0;
}
