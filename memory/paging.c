#include "paging.h"
#include "../kernel/mem.h"
#include "frame_alloc.h"
u32 *page_directory;
u32 *page_table;

int user_pages_ok(u32 ptr, u32 len) {
  if (len == 0)
    return 1;
  u32 end = ptr + len - 1;
  if (end < ptr)
    return 0;

  u32 addr = ptr & PAGE_ADDR_MASK;

  u32 last = end & PAGE_ADDR_MASK;

  while (1) {
    u32 pd_index = addr >> 22;
    u32 pt_index = (addr >> 12) & 0x3FF;

    u32 pde = page_directory[pd_index];

    if ((pde & PAGE_PRESENT) == 0)
      return 0;

    if ((pde & PAGE_USER) == 0)
      return 0;

    u32 *pt = (u32 *)(pde & PAGE_ADDR_MASK);
    u32 pte = pt[pt_index];

    if ((pte & PAGE_PRESENT) == 0)
      return 0;

    if ((pte & PAGE_USER) == 0)
      return 0;

    if (addr == last)
      break;

    if (addr > 0xFFFFFFFF - FRAME_SIZE)
      return 0;

    addr += FRAME_SIZE;
  }

  return 1;
}

void init_paging() {
  page_directory = (u32 *)alloc_frame();
  memory_set((u8 *)page_directory, 0, FRAME_SIZE);
  page_table = (u32 *)alloc_frame();

  for (int i = 0; i < PAGE_ENTRIES; i++) {
    page_table[i] = (i * FRAME_SIZE) | PAGE_PRESENT | PAGE_RW;
  }

  page_directory[0] = (u32)page_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
  load_page_directory(page_directory);
  enable_paging();
}

void mark_user_page(u32 virt) {
  u32 pd_index = virt >> 22;
  u32 pt_index = (virt >> 12) & 0x3FF;

  u32 pde = page_directory[pd_index];
  if ((pde & PAGE_PRESENT) == 0)
    return;

  u32 *pt = (u32 *)(pde & PAGE_ADDR_MASK);
  pt[pt_index] |= PAGE_USER;
}

void mark_user_range(u32 start, u32 len) {
  if (len == 0)
    return;

  u32 end = start + len - 1;
  if (end < start)
    return;

  u32 addr = start & PAGE_ADDR_MASK;
  u32 last = end & PAGE_ADDR_MASK;

  while (1) {
    mark_user_page(addr);

    if (addr == last)
      break;

    if (addr > 0xFFFFFFFF - FRAME_SIZE)
      return;

    addr += FRAME_SIZE;
  }
}
