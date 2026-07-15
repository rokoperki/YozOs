#include "cpu/gdt.h"
#include "cpu/isr.h"
#include "cpu/timer.h"
#include "drivers/keyboard.h"
#include "drivers/screen.h"
#include "fs/fat.h"
#include "fs/vfs.h"
#include "memory/frame_alloc.h"
#include "memory/paging.h"
#include "shell/shell.h"
#include "task/task.h"

int main() {
  clear_screen();

  gdt_install();
  isr_install();

  shell_print_prompt();
  init_keyboard();
  init_timer(50);

  init_frames();
  scheduler_init();
  init_paging();

  fs_init();
  vfs_init();

  __asm__ __volatile__("sti");

  while (1) {
    if (keyboard_line_ready() && keyboard_get_owner() == KEYBOARD_OWNER_SHELL) {
      user_input(keyboard_get_line());
      keyboard_clear_line();
      shell_print_prompt();
    }

    __asm__ __volatile__("hlt");
  }
}
