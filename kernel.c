#include "cpu/isr.h"
#include "cpu/memory_map.h"
#include "cpu/timer.h"
#include "drivers/keyboard.h"
#include "drivers/screen.h"
#include "kernel/string.h"

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

  __asm__ __volatile__("sti");
  return 0;
}

void user_input(char *input) {
  if (strcmp(input, "END") == 0) {
    print("Stopping the CPU. Bye!\n");
    asm volatile("hlt");
  } else if (strcmp(input, "MMAP") == 0) {
    memory_map_print();
  }
}
