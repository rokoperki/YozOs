#include "syscall.h"

static unsigned int strlen_user(const char *s) {
  unsigned int n = 0;
  while (s[n])
    n++;
  return n;
}

static void puts_user(const char *s) { sys_write(STDOUT, s, strlen_user(s)); }

static void put_uint(unsigned int value) {
  char buf[16];
  unsigned int i = sizeof(buf);

  buf[--i] = '\0';

  if (value == 0) {
    buf[--i] = '0';
  } else {
    while (value > 0 && i > 0) {
      buf[--i] = '0' + (value % 10);
      value /= 10;
    }
  }

  puts_user(&buf[i]);
}

int main(int argc, char **argv, char **envp) {
  (void)envp;

  puts_user("c argc=");
  put_uint((unsigned int)argc);
  puts_user("\n");

  for (int i = 0; i < argc; i++) {
    puts_user("c argv[");
    put_uint((unsigned int)i);
    puts_user("]=");
    puts_user(argv[i]);
    puts_user("\n");
  }

  return 0;
}
