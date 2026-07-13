#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

#define SYSCALL_INT 0x80
#define SYS_WRITE_CHAR 1
#define SYS_EXIT 2
#define SYS_STRING_WRITE 3
#define SYS_WRITE_BUFFER 4
#define SYS_READ_LINE 5
#define SYS_YIELD 6
#define SYS_GETPID 7
#define SYS_WAITPID 8
#define SYS_GETPPID 9
#define SYS_KILL 10
#define SYS_OPEN 11
#define SYS_READ 12
#define SYS_CLOSE 13
#define SYS_WRITE 14
#define SYS_STAT 15

extern void syscall_stub(void);

void syscall_install(void);
u32 syscall_handler(u32 num, u32 arg1, u32 arg2, u32 arg3);

#endif
