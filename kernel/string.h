#ifndef STRING
#define STRING

#include "../cpu/types.h"

void int_to_ascii(int n, char str[]);
void append(char s[], char n);
void backspace(char s[]);
int strlen(char s[]);
int strcmp(char s1[], char s2[]);

#endif
