#include "user_mode.h"

void user_main() {
  asm volatile("cli");
  while (1) {
  }
}
