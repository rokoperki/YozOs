#include "keyboard.h"
#include "../cpu/isr.h"
#include "../cpu/types.h"
#include "../kernel/util.h"
#include "low_level.h"
#include "screen.h"

static const char *scancode_names[] = {
    "ERROR",  "ESC",      "1",     "2",  "3", "4",         "5",      "6",  "7",
    "8",      "9",        "0",     "-",  "+", "Backspace", "Tab",    "Q",  "W",
    "E",      "R",        "T",     "Y",  "U", "I",         "O",      "P",  "[",
    "]",      "ENTER",    "LCtrl", "A",  "S", "D",         "F",      "G",  "H",
    "J",      "K",        "L",     ";",  "'", "`",         "LShift", "\\", "Z",
    "X",      "C",        "V",     "B",  "N", "M",         ",",      ".",  "/",
    "Rshift", "Keypad *", "LAlt",  "Spc"};

void print_letter(u8 scancode) {
  if (scancode <= 0x39) {
    print(scancode_names[scancode]);
  } else if (scancode <= 0x7f) {
    print("Unknown key down");
  } else if (scancode <= 0x39 + 0x80) {
    /* 'keyup' event corresponds to the 'keydown' + 0x80 */
    print("key up ");
    print_letter(scancode - 0x80);
  } else {
    print("Unknown key up");
  }
}
static void keyboard_callback(registers_t regs) {
  u8 scancode = port_byte_in(0x60);

  char scancode_ascii[4];
  int_to_ascii(scancode, scancode_ascii);
  print(scancode_ascii);
  print(", ");
  print_letter(scancode);
  print("\n");
}
void init_keyboard() { register_interrupt_handler(IRQ1, keyboard_callback); }
