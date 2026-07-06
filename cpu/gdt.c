#include "gdt.h"

#define GDT_ENTRIES 5

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;

static void gdt_set_gate(int num, u32 base, u32 limit, u8 access, u8 gran) {
  gdt_entry_t *entry = &gdt_entries[num];

  entry->base_low = base & 0xFFFF;
  entry->base_middle = (base >> 16) & 0xFF;
  entry->base_high = (base >> 24) & 0xFF;

  entry->limit_low = limit & 0xFFFF;
  entry->granularity = (limit >> 16) & 0x0F;
  entry->granularity |= gran & 0xF0;

  entry->access = access;
}

void gdt_install() {
  gdt_ptr.limit = sizeof(gdt_entry_t) * GDT_ENTRIES - 1;
  gdt_ptr.base = (u32)&gdt_entries;

  gdt_set_gate(0, 0, 0, 0, 0);
  gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
  gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);
  gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);
  gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);

  gdt_flush(&gdt_ptr);
}
