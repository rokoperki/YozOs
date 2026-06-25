#include "drivers/screen.h"

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
  print("             a hobby x86 OS from scratch   \n");
  print("  ======================================================= \n");
  print("\n");
  print("   [ ok ] boot sector\n");
  print("   [ ok ] protected mode\n");
  print("   [ ok ] screen driver online\n");
  print("\n");
  print("   YozOs> _\n");

  return 0;
}
