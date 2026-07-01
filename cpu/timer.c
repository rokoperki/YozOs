#include "timer.h"
#include "../drivers/low_level.h"
#include "../kernel/function.h"
#include "../task/task.h"
#include "isr.h"
#include "types.h"

u32 tick = 0;

static void timer_callback(registers_t regs) {
  tick++;
  if (tick % 10 == 0)
    schedule();
  UNUSED(regs);
}

void init_timer(u32 freq) {
  register_interrupt_handler(IRQ0, timer_callback);
  u32 divisor = 1193182 / freq;
  u8 low = (u8)(divisor & 0xFF);
  u8 high = (u8)((divisor >> 8) & 0xFF);

  // command: channel 0, lo/hi, mode 3-square wave generator
  port_byte_out(0x43, 0x36);
  port_byte_out(0x40, low);  // divisor low byte
  port_byte_out(0x40, high); // divisor high byte
}
