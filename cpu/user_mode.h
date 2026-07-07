#ifndef USER_MODE_H
#define USER_MODE_H

#include "types.h"

#define USER_STACK_TOP 0x80000

typedef struct {
  u32 start;
  u32 len;
} user_region_t;

typedef struct {
  u32 entry;
  u32 stack_top;
  user_region_t *regions;
  u32 region_count;
} user_program_t;

void enter_user_mode(u32 entry, u32 user_stack);
int run_user_program(user_program_t *program);

void user_exit_current(u32 code);
u32 user_context_save(void);
void user_context_restore(u32 code);
int run_user_test(void);

#endif
