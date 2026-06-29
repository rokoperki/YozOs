#include "memory_map.h"
#include "../drivers/screen.h"
#include "../kernel/string.h"

void print_u64(u64 number, char buf[]) {
  hex_to_ascii((u32)(number >> 32), buf);
  print(buf);
  hex_to_ascii((u32)number, buf);
  print(buf);
}

void memory_map_print() {
  int count = *(u16 *)MMAP_ENT_NUM;
  char count_ascii[100];
  e820_entry_t *entries = (e820_entry_t *)MMAP_ENT_START;

  int_to_ascii(count, count_ascii);
  print("Memory entries: count ");
  println(count_ascii);

  for (int i = 0; i < count; i++) {
    char buf[20];
    print("base: 0x");
    print_u64(entries[i].base, buf);

    print(" lenght: 0x");
    print_u64(entries[i].length, buf);

    switch (entries[i].type) {
    case USABLE:
      println(" usable-RAM");
      break;
    case RESERVED:
      println(" reserved");
      break;
    case ACPI_RECLAIMABLE:
      println(" ACPI reclaimable");
      break;
    case ACPI_NVS:
      println(" ACPI nvs");
      break;
    case BAD:
      println(" bad memory");
      break;
    default:
      println(" unkwonw type");
    }
  }
}
