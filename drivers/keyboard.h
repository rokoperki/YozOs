#ifndef KEYBOARD
#define KEYBOARD

typedef enum {
  KEYBOARD_OWNER_SHELL,
  KEYBOARD_OWNER_USER,
} keyboard_owner_t;

void keyboard_set_owner(keyboard_owner_t owner);
keyboard_owner_t keyboard_get_owner(void);

void init_keyboard();
int keyboard_line_ready(void);
char *keyboard_get_line(void);
void keyboard_clear_line(void);

#endif
