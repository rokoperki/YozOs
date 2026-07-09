
bits 32
org 0x70000

SYS_EXIT equ 2
SYS_GETPID equ 7
SYS_WAITPID equ 8 
WAITPID_RUNNING equ 0xFFFFFFFC 

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_GETPID
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov ebx, eax 
    mov eax, SYS_WAITPID
    xor ecx, ecx
    xor edx, edx
    int 0x80

    cmp eax, WAITPID_RUNNING
    je waitpid_ok

    mov ebx, eax 
    jmp exit 

waitpid_ok:
  xor ebx, ebx

exit: 
  mov eax, SYS_EXIT
  xor ecx, ecx
  xor edx, edx
  int 0x80

program_end:

