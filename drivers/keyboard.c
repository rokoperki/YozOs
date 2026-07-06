#include "keyboard.h"
#include "../cpu/isr.h"
#include "../cpu/types.h"
#include "../kernel/function.h"
#include "../kernel/string.h"
#include "../shell/shell.h"
#include "keyboard.h"
#include "low_level.h"
#include "screen.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C
#define SC_MAX 57
static char key_buffer[1024];

const char *sc_name[] = {
    "ERROR",     "Esc",     "1", "2", "3", "4",      "5",
    "6",         "7",       "8", "9", "0", "-",      "=",
    "Backspace", "Tab",     "Q", "W", "E", "R",      "T",
    "Y",         "U",       "I", "O", "P", "[",      "]",
    "Enter",     "Lctrl",   "A", "S", "D", "F",      "G",
    "H",         "J",       "K", "L", ";", "'",      "`",
    "LShift",    "\\",      "Z", "X", "C", "V",      "B",
    "N",         "M",       ",", ".", "/", "RShift", "Keypad *",
    "LAlt",      "Spacebar"};
const char sc_ascii[] = {
    '?', '?', '1', '2', '3', '4', '5', '6', '7', '8', '9',  '0', '-', '=',  '?',
    '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',  '[', ']', '?',  '?',
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z',
    'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', '?', '?',  '?', ' '};

static void keyboard_callback(registers_t regs) {
  u8 scancode = port_byte_in(0x60);

  if (scancode > SC_MAX)
    return;
  if (scancode == BACKSPACE) {
    int len = strlen(key_buffer);
    if (len > 0) {
      backspace(key_buffer);
      print_backspace();
    }
  } else if (scancode == ENTER) {
    print("\n");
    user_input(key_buffer);
    print("yozOS > ");
    key_buffer[0] = '\0';
  } else {
    char letter = sc_ascii[(int)scancode];
    append(key_buffer, letter);
    print_char(letter, -1, -1, WHITE_ON_BLACK);
  }

  UNUSED(regs);
}

void init_keyboard() { register_interrupt_handler(IRQ1, keyboard_callback); }
