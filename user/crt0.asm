bits 32 
global _start 
extern main 

%define SYS_EXIT 2 

section .text 
_start: 
  mov eax, [esp] ;argc 
  mov ebx, [esp + 4] ;argv 
  mov ecx, [esp + 8] ;envp 

  push ecx 
  push ebx 
  push eax 
  call main 

  add esp, 12

  mov ebx, eax 
  mov eax, SYS_EXIT
  int 0x80 

.hang:
  jmp .hang 
  
