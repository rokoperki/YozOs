#include "user_syscall.h"
#include "syscall.h"

static u32 syscall3(u32 num, u32 arg1, u32 arg2, u32 arg3) {
  u32 ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
  return ret;
}

u32 user_read_line(char *buff, u32 max_len) {
  return syscall3(SYS_READ_LINE, (u32)buff, max_len, 0);
}

u32 user_write_char(char c) { return syscall3(SYS_WRITE_CHAR, (u32)c, 0, 0); }

u32 user_write_string(char *str) {
  return syscall3(SYS_STRING_WRITE, (u32)str, 0, 0);
}

u32 user_write_buffer(char *buff, u32 len) {
  return syscall3(SYS_WRITE_BUFFER, (u32)buff, len, 0);
}

void user_exit(u32 code) {
  syscall3(SYS_EXIT, code, 0, 0);
  while (1) {
  }
}

u32 user_yield(void) { return syscall3(SYS_YIELD, 0, 0, 0); }
