#include "cpu/isr.h"
#include "cpu/timer.h"
#include "drivers/keyboard.h"
#include "drivers/screen.h"
#include "fs/fat.h"
#include "memory/frame_alloc.h"
#include "memory/paging.h"

int main() {
  clear_screen();

  print("\n");
  print("  __    __    ______   ________   ______    ______   \n");
  print(" /  |  /  |  /      \\ /        | /      \\  /      \\  \n");
  print(" $$ |  $$ | /$$$$$$  |$$$$$$$$/ /$$$$$$  |/$$$$$$  | \n");
  print(" $$ |  $$ | $$ |  $$ |   /$$/  $$ |  $$ |$$ \\__$$/  \n");
  print(" $$  \\/$$/  $$ |  $$ |  /$$/   $$ |  $$ |$$      \\   \n");
  print("  $$  $$<   $$ |  $$ | /$$/    $$ |  $$ | $$$$$$  |  \n");
  print("   $$$$  \\  $$ \\__$$ |/$$/____ $$ \\__$$ |/  \\__$$ |  \n");
  print("    $$  |   $$    $$/ $$       |$$    $$/ $$    $$/   \n");
  print("    $$/      $$$$$$/  $$$$$$$$/  $$$$$$/   $$$$$$/    \n");
  print("\n");
  print("  ======================================================= \n");
  print("  ======================================================= \n");
  print("\n");
  print("   [ ok ] boot sector\n");
  print("   [ ok ] protected mode\n");
  print("   [ ok ] screen driver online\n");
  print("\n");

  isr_install();

  print("yozOS > ");
  init_keyboard();
  init_timer(50);

  init_frames();
  init_paging();

  fs_init();

  __asm__ __volatile__("sti");
  return 0;
}
