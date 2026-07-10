#include "paging.h"
#include "../kernel/mem.h"
#include "frame_alloc.h"

static address_space_t kernel_space;
static u32 *kernel_page_table;
static address_space_t *current_space;

static address_space_t user_spaces[MAX_ADDRESS_SPACES];

address_space_t *kernel_address_space(void) { return &kernel_space; }

u32 *address_space_page_directory(address_space_t *space) {
  if (!space)
    return 0;

  return space->page_directory;
}

int address_space_user_pages_ok(address_space_t *space, u32 ptr, u32 len) {
  if (!space || !space->page_directory)
    return 0;

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

    u32 pde = space->page_directory[pd_index];

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

int user_pages_ok(u32 ptr, u32 len) {
  return address_space_user_pages_ok(current_space, ptr, len);
}

void init_paging() {
  kernel_space.page_directory = (u32 *)alloc_frame();
  memory_set((u8 *)kernel_space.page_directory, 0, FRAME_SIZE);
  kernel_page_table = (u32 *)alloc_frame();
  kernel_space.low_page_table = kernel_page_table;

  for (int i = 0; i < PAGE_ENTRIES; i++) {
    kernel_page_table[i] = (i * FRAME_SIZE) | PAGE_PRESENT | PAGE_RW;
  }

  kernel_space.page_directory[0] =
      (u32)kernel_page_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;

  current_space = &kernel_space;
  load_page_directory(kernel_space.page_directory);
  enable_paging();
}

static void address_space_mark_user_page(address_space_t *space, u32 virt) {
  if (!space || !space->page_directory)
    return;

  u32 pd_index = virt >> 22;
  u32 pt_index = (virt >> 12) & 0x3FF;

  u32 pde = space->page_directory[pd_index];
  if ((pde & PAGE_PRESENT) == 0)
    return;

  u32 *pt = (u32 *)(pde & PAGE_ADDR_MASK);
  u32 pte = pt[pt_index];
  if ((pte & PAGE_PRESENT) == 0)
    return;

  u32 new_flags = (pte & ~PAGE_ADDR_MASK) | PAGE_USER;
  address_space_map_page(space, virt, pte & PAGE_ADDR_MASK, new_flags);
}

void address_space_mark_user_range(address_space_t *space, u32 start, u32 len) {
  if (len == 0)
    return;

  u32 end = start + len - 1;
  if (end < start)
    return;

  u32 addr = start & PAGE_ADDR_MASK;
  u32 last = end & PAGE_ADDR_MASK;

  while (1) {
    address_space_mark_user_page(space, addr);

    if (addr == last)
      break;

    if (addr > 0xFFFFFFFF - FRAME_SIZE)
      return;

    addr += FRAME_SIZE;
  }
}

void mark_user_page(u32 virt) {
  address_space_mark_user_page(current_space, virt);
}

void mark_user_range(u32 start, u32 len) {
  address_space_mark_user_range(current_space, start, len);
}

address_space_t *address_space_create_user(void) {
  for (int i = 0; i < MAX_ADDRESS_SPACES; i++) {
    if (user_spaces[i].page_directory)
      continue;

    u32 dir = alloc_frame();

    if (dir == 0)
      return 0;

    user_spaces[i].page_directory = (u32 *)dir;
    memory_set((u8 *)user_spaces[i].page_directory, 0, FRAME_SIZE);

    u32 table = alloc_frame();
    if (table == 0) {
      free_frame(dir);
      user_spaces[i].page_directory = 0;
      return 0;
    }

    user_spaces[i].low_page_table = (u32 *)table;
    memory_set((u8 *)user_spaces[i].low_page_table, 0, FRAME_SIZE);

    memory_copy((char *)kernel_space.low_page_table,
                (char *)user_spaces[i].low_page_table, FRAME_SIZE);

    user_spaces[i].page_directory[0] =
        (u32)user_spaces[i].low_page_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;

    return &user_spaces[i];
  }
  return 0;
}

void address_space_destroy(address_space_t *space) {
  if (!space || space == &kernel_space)
    return;

  if (space->low_page_table) {
    free_frame((u32)space->low_page_table);
    space->low_page_table = 0;
  }

  if (space->page_directory) {
    free_frame((u32)space->page_directory);
    space->page_directory = 0;
  }
}

void address_space_switch(address_space_t *space) {
  if (!space || !space->page_directory)
    return;

  current_space = space;
  load_page_directory(space->page_directory);
}

int address_space_map_page(address_space_t *space, u32 virt, u32 phys,
                           u32 flags) {
  if (!space || !space->page_directory)
    return 0;

  u32 pd_index = virt >> 22;
  u32 pt_index = (virt >> 12) & 0x3FF;

  u32 pde = space->page_directory[pd_index];
  if ((pde & PAGE_PRESENT) == 0)
    return 0;

  u32 *pt = (u32 *)(pde & PAGE_ADDR_MASK);
  pt[pt_index] = (phys & PAGE_ADDR_MASK) | flags | PAGE_PRESENT;

  return 1;
}
