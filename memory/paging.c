#include "paging.h"
#include "../kernel/mem.h"
#include "frame_alloc.h"
u32 *page_directory;
u32 *page_table;

void init_paging() {
  page_directory = (u32 *)alloc_frame();
  memory_set((u8 *)page_directory, 0, FRAME_SIZE);
  page_table = (u32 *)alloc_frame();

  for (int i = 0; i < PAGE_ENTRIES; i++) {
    page_table[i] = (i * FRAME_SIZE) | PAGE_PRESENT | PAGE_RW;
  }

  page_directory[0] = (u32)page_table | PAGE_PRESENT | PAGE_RW;
  load_page_directory(page_directory);
  enable_paging();
}
