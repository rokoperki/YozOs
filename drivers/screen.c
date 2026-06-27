#include "screen.h"
#include "low_level.h"

/* Map (col, row) coordinates to a byte offset from the start of video memory.
 * Each character cell is two bytes, hence the final * 2. */
int get_screen_offset(int col, int row) { return (row * MAX_COLS + col) * 2; }

/* Print a char on the screen at col, row, or at the cursor position. */
void print_char(char character, int col, int row, char attribute_byte) {
  unsigned char *vidmem = (unsigned char *)VIDEO_ADDRESS;

  if (!attribute_byte) {
    attribute_byte = WHITE_ON_BLACK;
  }

  int offset;
  if (col >= 0 && row >= 0) {
    offset = get_screen_offset(col, row);
  } else {
    offset = get_cursor();
  }

  // For a newline, jump to the end of the current row so the cell advance
  // below lands on the first column of the next row.
  if (character == '\n') {
    int rows = offset / (2 * MAX_COLS);
    offset = get_screen_offset(MAX_COLS - 1, rows);
  } else {
    vidmem[offset] = character;
    vidmem[offset + 1] = attribute_byte;
  }

  offset += 2; // advance to the next cell (two bytes)
  offset = handle_scrolling(offset);
  set_cursor(offset);
}

int get_cursor() {
  // The device uses its control register as an index to select its internal
  // registers, of which we are interested in:
  //   reg 14: high byte of the cursor's offset
  //   reg 15: low byte of the cursor's offset
  // Once selected, we read/write a byte on the data register.
  port_byte_out(REG_SCREEN_CTRL, 14);
  int offset = port_byte_in(REG_SCREEN_DATA) << 8;
  port_byte_out(REG_SCREEN_CTRL, 15);
  offset += port_byte_in(REG_SCREEN_DATA);

  // The cursor offset reported by the VGA hardware is a count of characters,
  // so multiply by two to convert it to a (byte) cell offset.
  return offset * 2;
}

void set_cursor(int offset) {
  offset /= 2; // Convert from cell (byte) offset to char offset.

  port_byte_out(REG_SCREEN_CTRL, 14);
  port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
  port_byte_out(REG_SCREEN_CTRL, 15);
  port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}

/* Advance the screen by one row when the cursor runs off the bottom. */
int handle_scrolling(int offset) {
  if (offset < MAX_ROWS * MAX_COLS * 2) {
    return offset;
  }

  // Shuffle every row up by one (row i-1 <- row i).
  unsigned char *vidmem = (unsigned char *)VIDEO_ADDRESS;
  int i;
  for (i = 1; i < MAX_ROWS; i++) {
    int from = get_screen_offset(0, i);
    int to = get_screen_offset(0, i - 1);
    int b;
    for (b = 0; b < MAX_COLS * 2; b++) {
      vidmem[to + b] = vidmem[from + b];
    }
  }

  // Blank the last row.
  int last = get_screen_offset(0, MAX_ROWS - 1);
  for (i = 0; i < MAX_COLS * 2; i++) {
    vidmem[last + i] = 0;
  }

  // Pull the offset back onto the last visible row.
  offset -= 2 * MAX_COLS;
  return offset;
}

/* Print a string from (col, row); pass (-1, -1) to start at the cursor. */
void print_at(const char *message, int col, int row) {
  if (col >= 0 && row >= 0) {
    set_cursor(get_screen_offset(col, row));
  }

  int i = 0;
  while (message[i] != 0) {
    print_char(message[i++], col, row, WHITE_ON_BLACK);
  }
}

/* Convenience wrapper: print at the current cursor position. */
void print(const char *message) { print_at(message, -1, -1); }

/* Tidy the screen by writing blank characters at every position. */
void clear_screen() {
  int row = 0;
  int col = 0;

  for (row = 0; row < MAX_ROWS; row++) {
    for (col = 0; col < MAX_COLS; col++) {
      print_char(' ', col, row, WHITE_ON_BLACK);
    }
  }

  set_cursor(get_screen_offset(0, 0));
}
