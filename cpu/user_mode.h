#ifndef USER_MODE_H
#define USER_MODE_H

#include "types.h"

#define USER_STACK_TOP 0x80000

void enter_user_mode(u32 entry, u32 user_stack);
void user_main(void);

void user_exit_current(u32 code);
u32 user_context_save(void);
void user_context_restore(u32 code);
int run_user_test(void);

#endif
