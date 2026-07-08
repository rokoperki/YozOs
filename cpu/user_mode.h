#ifndef USER_MODE_H
#define USER_MODE_H

#include "types.h"

#define USER_STACK_TOP 0x80000
#define USER_EXIT_FAULT 0xFFFFFFFE

typedef struct {
  u32 start;
  u32 len;
} user_region_t;

typedef enum {
  USER_PROCESS_UNUSED,
  USER_PROCESS_READY,
  USER_PROCESS_RUNNING,
  USER_PROCESS_EXITED,
  USER_PROCESS_FAILED,
} user_process_state_t;

typedef struct {
  u32 entry;
  u32 stack_top;
  user_region_t *regions;
  u32 region_count;
} user_program_t;

typedef struct {
  const char *name;
  user_program_t *program;
  user_process_state_t state;
  u32 exit_code;
} user_process_t;

int run_user_process(user_process_t *process);
void enter_user_mode(u32 entry, u32 user_stack);
int run_user_program(user_program_t *program);

void user_exit_current(u32 code);
u32 user_context_save(void);
void user_context_restore(u32 code);
int run_user_test(void);

void user_fault_current(void);

int run_user_fault_test(void);
void user_test_task_entry(void);
void user_fault_task_entry(void);
void user_process_dump(void);
void user_process_reap(void);

#define MAX_USER_PROCESSES 8

#endif
