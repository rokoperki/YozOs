#include "user_mode.h"
#include "../drivers/screen.h"
#include "../kernel/function.h"
#include "syscall.h"
#include "tss.h"

#define USER_KERNEL_STACK_SIZE 4096

static u8 user_kernel_stack[USER_KERNEL_STACK_SIZE];

static u32 syscall3(u32 num, u32 arg1, u32 arg2, u32 arg3) {
  u32 ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
  return ret;
}

static void sys_write_char(char c) { syscall3(SYS_WRITE_CHAR, (u32)c, 0, 0); }

static void sys_exit(u32 code) {
  syscall3(SYS_EXIT, code, 0, 0);
  while (1) {
  }
}

void user_main() {
  sys_write_char('U');
  sys_exit(0);
}

void user_exit_current(u32 code) { user_context_restore(code + 1); }

int run_user_test() {
  u32 ret = user_context_save();

  if (ret != 0) {
    tss_set_kernel_stack(0x90000);
    return ret - 1;
  }

  tss_set_kernel_stack((u32)user_kernel_stack + USER_KERNEL_STACK_SIZE);
  enter_user_mode((u32)user_main, USER_STACK_TOP);

  tss_set_kernel_stack(0x90000);
  return -1;
}
