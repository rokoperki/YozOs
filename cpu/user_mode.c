#include "user_mode.h"
#include "../drivers/keyboard.h"
#include "../memory/paging.h"
#include "tss.h"
#include "user_test.h"

static void prepare_user_program(user_program_t *program) {
  for (u32 i = 0; i < program->region_count; i++) {
    mark_user_range(program->regions[i].start, program->regions[i].len);
  }
}

#define USER_KERNEL_STACK_SIZE 4096

static u8 user_kernel_stack[USER_KERNEL_STACK_SIZE];

int run_user_program(user_program_t *program) {
  u32 ret = user_context_save();

  if (ret != 0) {
    keyboard_set_owner(KEYBOARD_OWNER_SHELL);
    tss_set_kernel_stack(0x90000);
    __asm__ __volatile__("sti");
    return ret - 1;
  }
  keyboard_clear_line();
  keyboard_set_owner(KEYBOARD_OWNER_USER);
  keyboard_clear_line();
  prepare_user_program(program);
  tss_set_kernel_stack((u32)user_kernel_stack + USER_KERNEL_STACK_SIZE);
  enter_user_mode(program->entry, program->stack_top);

  tss_set_kernel_stack(0x90000);
  return -1;
}

void user_exit_current(u32 code) { user_context_restore(code + 1); }

int run_user_test() {
  user_region_t regions[] = {
      {(u32)user_test_main, FRAME_SIZE},
      {(u32)user_test_msg, user_test_msg_len},
      {(u32)user_test_bad_ptr_msg, user_test_bad_ptr_msg_len},
      {(u32)user_test_input_buf, user_test_input_buf_len},
      {(u32)user_test_prompt, user_test_prompt_len},
      {(u32)user_test_got_msg, user_test_got_msg_len},
      {USER_STACK_TOP - FRAME_SIZE, FRAME_SIZE},
  };

  user_program_t program = {.entry = (u32)user_test_main,
                            .stack_top = USER_STACK_TOP,
                            .regions = regions,
                            .region_count =
                                sizeof(regions) / sizeof(regions[0])};

  return run_user_program(&program);
}
