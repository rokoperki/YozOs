#include "user_mode.h"
#include "../memory/paging.h"
#include "syscall.h"
#include "tss.h"

typedef struct {
  u32 entry;
  u32 code_start;
  u32 code_len;
  u32 data_start;
  u32 data_len;
  u32 stack_top;
  u32 stack_len;
} user_program_t;

static void prepare_user_program(user_program_t *program) {
  mark_user_range(program->code_start, program->code_len);
  mark_user_range(program->data_start, program->data_len);
  mark_user_range(program->stack_top - program->stack_len, program->stack_len);
}

#define USER_KERNEL_STACK_SIZE 4096

static u8 user_kernel_stack[USER_KERNEL_STACK_SIZE];
static char user_msg[] = "ser mode says hi\n";
static char bad_ptr_msg[] = "bad pointer rejected\n";
static u32 syscall3(u32 num, u32 arg1, u32 arg2, u32 arg3) {
  u32 ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
  return ret;
}

static u32 sys_write_char(char c) {
  return syscall3(SYS_WRITE_CHAR, (u32)c, 0, 0);
}

static u32 sys_string_write(char *str) {
  return syscall3(SYS_STRING_WRITE, (u32)str, 0, 0);
}

static void sys_exit(u32 code) {
  syscall3(SYS_EXIT, code, 0, 0);
  while (1) {
  }
}

static u32 sys_write_buffer(char *buff, u32 len) {
  return syscall3(SYS_WRITE_BUFFER, (u32)buff, len, 0);
}

void user_main() {
  sys_write_char('U');
  sys_string_write(user_msg);

  u32 ret = sys_write_buffer((char *)0xFFFFFFFF, 5);
  if (ret == 0xFFFFFFFF) {
    sys_string_write(bad_ptr_msg);
  }
  sys_exit(0);
}

void user_exit_current(u32 code) { user_context_restore(code + 1); }

int run_user_test() {
  u32 ret = user_context_save();

  if (ret != 0) {
    tss_set_kernel_stack(0x90000);
    return ret - 1;
  }

  user_program_t program = {
      .entry = (u32)user_main,
      .code_start = (u32)user_main,
      .code_len = FRAME_SIZE,
      .data_start = (u32)user_msg,
      .data_len = sizeof(user_msg) + sizeof(bad_ptr_msg),
      .stack_top = USER_STACK_TOP,
      .stack_len = FRAME_SIZE,
  };

  prepare_user_program(&program);
  tss_set_kernel_stack((u32)user_kernel_stack + USER_KERNEL_STACK_SIZE);
  enter_user_mode(program.entry, program.stack_top);

  tss_set_kernel_stack(0x90000);
  return -1;
}
