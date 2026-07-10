#ifndef USER_MODE_H
#define USER_MODE_H

#include "../memory/paging.h"
#include "types.h"

struct task;

#define USER_STACK_TOP 0x80000
#define USER_EXIT_FAULT 0xFFFFFFFE
#define USER_EXIT_KILLED 0xFFFFFFFD
#define USER_LOAD_ADDR 0x70000
#define USER_LOAD_MAX_BYTES 8192
#define USER_BIN_MAGIC 0x315A4F59
#define USER_BIN_HEADER_SIZE 16

#define USER_REGION_READ 1
#define USER_REGION_WRITE 2
#define USER_REGION_EXEC 4

#define USER_REAP_OK 1
#define USER_REAP_NOT_FOUND 0
#define USER_REAP_RUNNING -1

#define USER_KILL_OK 1
#define USER_KILL_NOT_FOUND 0
#define USER_KILL_DEAD 2
#define USER_KILL_NO_TASK 3

#define USER_WAIT_OK 1
#define USER_WAIT_NOT_FOUND 0
#define USER_WAIT_RUNNING -1

#define USER_WAITPID_RUNNING 0xFFFFFFFC
#define USER_WAITPID_NOT_FOUND 0xFFFFFFFF

#define USER_MAX_IMAGE_FRAMES 2

typedef struct {
  u32 start;
  u32 len;
  u32 flags;
} user_region_t;

typedef enum {
  USER_PROCESS_UNUSED,
  USER_PROCESS_READY,
  USER_PROCESS_RUNNING,
  USER_PROCESS_EXITED,
  USER_PROCESS_FAILED,
  USER_PROCESS_KILLED,
} user_process_state_t;

typedef struct {
  u32 entry;
  u32 stack_top;
  user_region_t *regions;
  u32 region_count;
} user_program_t;

typedef struct user_process {
  const char *name;
  u32 pid;
  u32 parent_pid;
  struct task *task;
  user_program_t *program;
  u32 user_stack_frame;
  address_space_t *address_space;
  user_process_state_t state;
  u32 exit_code;
  u32 image_frames[USER_MAX_IMAGE_FRAMES];
  u32 image_frame_count;
  user_region_t loaded_regions[2];
  user_program_t loaded_program;
} user_process_t;

int run_user_process(user_process_t *process);
void enter_user_mode(u32 entry, u32 user_stack);
int run_user_program(address_space_t *space, user_program_t *program);

void user_exit_current(u32 code);
u32 user_context_save(void);
void user_context_restore(u32 code);
int run_user_test(void);

void user_fault_current(void);

int run_user_fault_test(void);
struct task *start_user_test_task(void);
struct task *start_user_fault_task(void);
void user_test_task_entry(void);
void user_fault_task_entry(void);
void user_process_dump(void);
void user_process_reap(void);
int user_process_reap_pid(u32 pid);
int user_process_kill_pid(u32 pid);
u32 user_process_pid(struct user_process *process);
u32 user_current_pid(void);
u32 user_current_ppid();
int user_process_wait_pid(u32 pid);
u32 user_waitpid_status(u32 pid);

int user_memory_ok(u32 ptr, u32 len, u32 required_flags);
int run_user_file(char *name);

#define MAX_USER_PROCESSES 8

#endif
