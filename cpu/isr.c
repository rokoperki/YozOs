#include "isr.h"
#include "../drivers/low_level.h"
#include "../drivers/screen.h"
#include "../kernel/string.h"
#include "../memory/memory_map.h"
#include "../memory/paging.h"
#include "idt.h"
#include "syscall.h"
#include "user_mode.h"

isr_t interrupt_handlers[256];

void isr_install() {
  set_idt_gate(0, (u32)isr0);
  set_idt_gate(1, (u32)isr1);
  set_idt_gate(2, (u32)isr2);
  set_idt_gate(3, (u32)isr3);
  set_idt_gate(4, (u32)isr4);
  set_idt_gate(5, (u32)isr5);
  set_idt_gate(6, (u32)isr6);
  set_idt_gate(7, (u32)isr7);
  set_idt_gate(8, (u32)isr8);
  set_idt_gate(9, (u32)isr9);
  set_idt_gate(10, (u32)isr10);
  set_idt_gate(11, (u32)isr11);
  set_idt_gate(12, (u32)isr12);
  set_idt_gate(13, (u32)isr13);
  set_idt_gate(14, (u32)isr14);
  set_idt_gate(15, (u32)isr15);
  set_idt_gate(16, (u32)isr16);
  set_idt_gate(17, (u32)isr17);
  set_idt_gate(18, (u32)isr18);
  set_idt_gate(19, (u32)isr19);
  set_idt_gate(20, (u32)isr20);
  set_idt_gate(21, (u32)isr21);
  set_idt_gate(22, (u32)isr22);
  set_idt_gate(23, (u32)isr23);
  set_idt_gate(24, (u32)isr24);
  set_idt_gate(25, (u32)isr25);
  set_idt_gate(26, (u32)isr26);
  set_idt_gate(27, (u32)isr27);
  set_idt_gate(28, (u32)isr28);
  set_idt_gate(29, (u32)isr29);
  set_idt_gate(30, (u32)isr30);
  set_idt_gate(31, (u32)isr31);

  // REMAP THE PIC
  port_byte_out(0x20, 0x11); // master init begin
  port_byte_out(0xA0, 0x11); // slave init begin
  port_byte_out(0x21, 0x20); // master: IRQ0–7  → INT 0x20–0x27
  port_byte_out(0xA1, 0x28); // slave:  IRQ8–15 → INT 0x28–0x2F
  port_byte_out(0x21, 0x04); // master: slave is wired to IRQ2 {2 bit set}
  port_byte_out(0xA1, 0x02); // slave: my cascade entity is 2
  port_byte_out(0x21, 0x01); // master: 8086 mode
  port_byte_out(0xA1, 0x01); // slave: 8086 mode
  port_byte_out(0x21, 0x0);  // master: all ports enabled
  port_byte_out(0xA1, 0x0);  // slave: all ports enabled

  // Install the IRQs
  set_idt_gate(32, (u32)irq0);
  set_idt_gate(33, (u32)irq1);
  set_idt_gate(34, (u32)irq2);
  set_idt_gate(35, (u32)irq3);
  set_idt_gate(36, (u32)irq4);
  set_idt_gate(37, (u32)irq5);
  set_idt_gate(38, (u32)irq6);
  set_idt_gate(39, (u32)irq7);
  set_idt_gate(40, (u32)irq8);
  set_idt_gate(41, (u32)irq9);
  set_idt_gate(42, (u32)irq10);
  set_idt_gate(43, (u32)irq11);
  set_idt_gate(44, (u32)irq12);
  set_idt_gate(45, (u32)irq13);
  set_idt_gate(46, (u32)irq14);
  set_idt_gate(47, (u32)irq15);

  syscall_install();
  set_idt();
}

char *exception_messages[] = {"Division By Zero",
                              "Debug",
                              "Non Maskable Interrupt",
                              "Breakpoint",
                              "Into Detected Overflow",
                              "Out of Bounds",
                              "Invalid Opcode",
                              "No Coprocessor",

                              "Double Fault",
                              "Coprocessor Segment Overrun",
                              "Bad TSS",
                              "Segment Not Present",
                              "Stack Fault",
                              "General Protection Fault",
                              "Page Fault",
                              "Unknown Interrupt",

                              "Coprocessor Fault",
                              "Alignment Check",
                              "Machine Check",
                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved",

                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved",
                              "Reserved"};

void isr_handler(registers_t r) {
  if (r.int_no == 14) {
    print("Page fault addr: ");
    char buf[50];
    print_u64(read_cr2(), buf);
    println("");

    print("EIP: ");
    print_u64(r.eip, buf);
    println("");

    print("ERR: ");
    print_u64(r.err_code, buf);
    println("");

    print("CS: ");
    print_u64(r.cs, buf);
    println("");

    if ((r.cs & 0x3) == 3) {
      println("fault origin: user");
      user_fault_current();
      return;
    }

    println("fault origin: kernel");
    asm volatile(
        "cli; hlt"); // stop here so we don't loop on the faulting instr
  }

  print("received interrupt: ");
  char s[3];
  int_to_ascii(r.int_no, s);
  print(s);
  print("\n");
  print(exception_messages[r.int_no]);
  print("\n");
}
void register_interrupt_handler(u8 n, isr_t handler) {
  interrupt_handlers[n] = handler;
}

void irq_handler(registers_t r) {
  // send EOI to PCI
  if (r.int_no >= 40)
    port_byte_out(0xA0, 0x20); /* slave */
  port_byte_out(0x20, 0x20);   /* master */

  /* Handle the interrupt in a more modular way */
  if (interrupt_handlers[r.int_no] != 0) {
    isr_t handler = interrupt_handlers[r.int_no];
    handler(r);
  }
}
