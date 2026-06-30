#ifndef MEMORY
#define MEMORY

#include "../cpu/types.h"

typedef struct {
  u64 base;
  u64 length;
  u32 type; // 1 = usable RAM, 2 = reserved, 3/4 ACPI, 5 = bad
  u32 acpi; // extended attributes (the ACPI 3.x word)
} __attribute__((packed)) e820_entry_t;

#define MMAP_ENT_NUM 0x8000
#define MMAP_ENT_START 0x8004

void print_u64(u64 number, char buf[]);

typedef enum {
  USABLE = 1,
  RESERVED = 2,
  ACPI_RECLAIMABLE = 3,
  ACPI_NVS = 4,
  BAD = 5
} Mem_Ent_Type;

void memory_map_print();

#endif
