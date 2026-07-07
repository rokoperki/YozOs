#include "syscall.h"
#include "../drivers/screen.h"
#include "idt.h"
#include "user_mode.h"

void syscall_install(void) {
  set_idt_gate_flags(SYSCALL_INT, (u32)syscall_stub, 0xEE);
}

u32 syscall_handler(u32 num, u32 arg1, u32 arg2, u32 arg3) {
  if (num == SYS_WRITE_CHAR) {
    print_char((char)arg1, -1, -1, WHITE_ON_BLACK);
    return 0;
  }
  if (num == SYS_EXIT) {
    user_exit_current(arg1);
    return 0;
  }

  return 0xFFFFFFFF;
}
