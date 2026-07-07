#ifndef KEYBOARD
#define KEYBOARD

void init_keyboard();
int keyboard_line_ready(void);
char *keyboard_get_line(void);
void keyboard_clear_line(void);

#endif
