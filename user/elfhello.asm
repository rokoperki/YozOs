bits 32 
global _start 

section .text
_start:
  mov eax, 14
  mov ebx, 1 
  mov ecx, msg 
  mov edx, msg_len 
  int 0x80

  mov eax, 2 
  xor ebx, ebx 
  int 0x80

section .rodata 
msg:
  db "hello from ELF", 10 
msg_len: equ $ - msg
