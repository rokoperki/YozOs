#include "user_mode.h"

void user_main() {
  asm volatile("mov $1, %%eax\n"
               "mov $'U', %%ebx\n"
               "int $0x80\n"
               :
               :
               : "eax", "ebx");

  while (1) {
  }
}
