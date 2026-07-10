#ifndef PAGING
#define PAGING

#include "../cpu/types.h"
#include "frame_alloc.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_ADDR_MASK 0xFFFFF000
#define PAGE_ENTRIES 1024

#define MAX_ADDRESS_SPACES 8

typedef struct {
  u32 *page_directory;
} address_space_t;

void init_paging();
void map_page(u32 virt, u32 phys, u32 flags);
extern void load_page_directory(u32 *dir);
extern void enable_paging(void);
extern u32 read_cr2(void);

int user_pages_ok(u32 ptr, u32 len);
void mark_user_page(u32 virt);
void mark_user_range(u32 start, u32 len);

address_space_t *kernel_address_space(void);
address_space_t *address_space_create_user(void);
void address_space_destroy(address_space_t *space);
u32 *address_space_page_directory(address_space_t *space);
void address_space_switch(address_space_t *space);

#endif
