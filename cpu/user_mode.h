#ifndef USER_MODE_H
#define USER_MODE_H

#include "types.h"

#define USER_STACK_TOP 0x80000

void enter_user_mode(u32 entry, u32 user_stack);
void user_main(void);

#endif
