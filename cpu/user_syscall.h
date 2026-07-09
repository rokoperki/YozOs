#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include "types.h"

u32 user_read_line(char *buff, u32 max_len);
u32 user_write_char(char c);
u32 user_write_string(char *str);
u32 user_write_buffer(char *buff, u32 len);
void user_exit(u32 code);
u32 user_yield(void);
u32 sys_getpid(void);
u32 sys_getppid(void);
u32 sys_waitpid(u32 pid);
u32 sys_kill(u32 pid);

#endif
