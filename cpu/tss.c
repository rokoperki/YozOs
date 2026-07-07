#include "tss.h"
#include "../kernel/mem.h"
#include "gdt.h"

static tss_entry_t tss_entry;

void tss_install() {
  memory_set((u8 *)&tss_entry, 0, sizeof(tss_entry_t));
  tss_entry.ss0 = KERNEL_DATA_SEG;
  tss_entry.esp0 = 0x90000;
  tss_entry.iomap_base = sizeof(tss_entry_t);
}

tss_entry_t *tss_get_entry() { return &tss_entry; }

void tss_set_kernel_stack(u32 esp0) { tss_entry.esp0 = esp0; }
