#ifndef IDT_H
#define IDT_H

#include "types.h"

#define KERNEL_CS 0x08

typedef struct {
  u16 low_offset; /* bits 0-15 of handler address */
  u16 sel;        /* kernel segment selector */
  u8 always0;
  u8 flags; /* 0x8E = present, ring 0, 32-bit interrupt gate */
  u16 high_offset; /* bits 16-31 of handler address */
} __attribute__((packed)) idt_gate_t;

/* Passed to lidt. */
typedef struct {
  u16 limit;
  u32 base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256
extern idt_gate_t idt[IDT_ENTRIES];
extern idt_register_t idt_reg;

void set_idt_gate(int n, u32 handler);
void set_idt();

#endif
