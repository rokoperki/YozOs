#include "syscall.h"
#include "../drivers/keyboard.h"
#include "../drivers/screen.h"
#include "../fs/vfs.h"
#include "../kernel/string.h"
#include "../task/task.h"
#include "idt.h"
#include "user_mode.h"

#define MAX_USER_STRING 256
#define MAX_USER_BUFFER 256

static int user_cstring_ok(u32 ptr) {
  if (!user_memory_ok(ptr, 1, USER_REGION_READ))
    return 0;

  for (u32 i = 0; i < MAX_USER_STRING; i++) {
    u32 addr = ptr + i;
    if (addr < ptr)
      return 0;
    if (!user_memory_ok(addr, 1, USER_REGION_READ))
      return 0;
    if (((char *)addr)[0] == '\0')
      return 1;
  }

  return 0;
}

void syscall_install(void) {
  set_idt_gate_flags(SYSCALL_INT, (u32)syscall_stub, 0xEE);
}

static int copy_user_cstring(u32 ptr, char *dst, u32 max_len) {
  if (!user_cstring_ok(ptr))
    return 0;

  for (u32 i = 0; i < max_len; i++) {
    dst[i] = ((char *)ptr)[i];

    if (dst[i] == '\0')
      return 1;
  }

  dst[max_len - 1] = '\0';
  return 0;
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

  if (num == SYS_STRING_WRITE) {
    if (!user_cstring_ok(arg1))
      return 0xFFFFFFFF;
    print((char *)arg1);
    return 0;
  }

  if (num == SYS_WRITE_BUFFER) {
    if (arg2 > MAX_USER_BUFFER)
      return 0xFFFFFFFF;
    if (!user_memory_ok(arg1, arg2, USER_REGION_READ))
      return 0xFFFFFFFF;

    for (u32 i = 0; i < arg2; i++) {
      print_char(((char *)arg1)[i], -1, -1, WHITE_ON_BLACK);
    }
    return 0;
  }

  if (num == SYS_READ_LINE) {
    if (arg2 == 0 || arg2 > 256)
      return 0xFFFFFFFF;
    if (!user_memory_ok(arg1, arg2, USER_REGION_WRITE))
      return 0xFFFFFFFF;

    if (!keyboard_line_ready())
      return 0;

    char *src = keyboard_get_line();
    char *dst = (char *)arg1;

    u32 i = 0;
    while (i + 1 < arg2 && src[i]) {
      dst[i] = src[i];
      i++;
    }
    dst[i] = '\0';

    keyboard_clear_line();
    return i;
  }

  if (num == SYS_YIELD) {
    schedule();
    return 0;
  }

  if (num == SYS_GETPID) {
    return user_current_pid();
  }

  if (num == SYS_WAITPID) {
    return user_waitpid_status(arg1);
  }

  if (num == SYS_GETPPID) {
    return user_current_ppid();
  }

  if (num == SYS_KILL) {
    return user_process_kill_pid(arg1);
  }

  if (num == SYS_OPEN) {
    char path[VFS_MAX_PATH];

    if (!copy_user_cstring(arg1, path, VFS_MAX_PATH))
      return 0xFFFFFFFF;

    return user_fd_open_current(path);
  }

  if (num == SYS_READ) {
    if (arg3 > MAX_USER_BUFFER)
      return 0xFFFFFFFF;

    if (!user_memory_ok(arg2, arg3, USER_REGION_WRITE))
      return 0xFFFFFFFF;

    return user_fd_read_current((int)arg1, (u8 *)arg2, arg3);
  }

  if (num == SYS_CLOSE) {
    return user_fd_close_current((int)arg1);
  }

  return 0xFFFFFFFF;
}
