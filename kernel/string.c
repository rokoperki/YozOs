#include "string.h"

int strlen(char s[]) {
  int i = 0;
  while (s[i] != '\0')
    ++i;
  return i;
}

void reverse(char s[]) {
  int c, i, j;
  for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

void int_to_ascii(int n, char str[]) {
  int i, sign;
  if ((sign = n) < 0)
    n = -n;
  i = 0;
  do {
    str[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);

  if (sign < 0)
    str[i++] = '-';
  str[i] = '\0';

  reverse(str);
}

void append(char s[], char n) {
  int len = strlen(s);
  s[len] = n;
  s[len + 1] = '\0';
}

void backspace(char s[]) {
  int len = strlen(s);
  s[len - 1] = '\0';
}

int strcmp(char s1[], char s2[]) {
  int i;
  for (i = 0; s1[i] == s2[i]; i++) {
    if (s1[i] == '\0')
      return 0;
  }
  return s1[i] - s2[i];
}

int starts_with(char s[], char prefix[]) {
  for (int i = 0; prefix[i] != '\0'; i++) {
    if (s[i] != prefix[i])
      return 0;
  }
  return 1;
}

void hex_to_ascii(u32 n, char s[]) {
  for (int i = 0; i < 8; i++) {
    int shift = 28 - i * 4;
    int nibble = (n >> shift) & 0xF;
    char hex[] = "0123456789ABCDEF";
    s[i] = hex[nibble];
  }
  s[8] = '\0';
}

void hex16_to_ascii(u16 n, char s[]) {
  for (int i = 0; i < 4; i++) {
    int shift = 12 - i * 4;
    int nibble = (n >> shift) & 0xF;
    char hex[] = "0123456789ABCDEF";
    s[i] = hex[nibble];
  }
  s[4] = '\0';
}
