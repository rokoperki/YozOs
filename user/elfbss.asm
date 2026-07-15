bits 32
global _start

section .text
_start:
  cmp dword [counter], 0
  jne .bad

  mov dword [counter], 123
  cmp dword [counter], 123
  jne .bad

  mov eax, 14
  mov ebx, 1
  mov ecx, ok_msg
  mov edx, ok_len
  int 0x80

  mov eax, 2
  xor ebx, ebx
  int 0x80

.bad:
  mov eax, 14
  mov ebx, 1
  mov ecx, bad_msg
  mov edx, bad_len
  int 0x80

  mov eax, 2
  mov ebx, 1
  int 0x80

section .bss
counter:
  resd 1

section .rodata
ok_msg:
  db "bss ok", 10
ok_len: equ $ - ok_msg

bad_msg:
  db "bss bad", 10
bad_len: equ $ - bad_msg
