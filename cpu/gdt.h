#ifndef GDT_H
#define GDT_H

#include "types.h"

#define KERNEL_CODE_SEG 0x08
#define KERNEL_DATA_SEG 0x10
#define USER_CODE_SEG 0x1B
#define USER_DATA_SEG 0x23

typedef struct {
  u16 limit_low;
  u16 base_low;
  u8 base_middle;
  u8 access;
  u8 granularity;
  u8 base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
  u16 limit;
  u32 base;
} __attribute__((packed)) gdt_ptr_t;

void gdt_install(void);
void gdt_flush(gdt_ptr_t *ptr);

#endif
