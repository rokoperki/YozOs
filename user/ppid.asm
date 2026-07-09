bits 32
org 0x70000

SYS_EXIT equ 2
SYS_GETPPID equ 9

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_GETPPID
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov ebx, eax
    mov eax, SYS_EXIT
    xor ecx, ecx
    xor edx, edx
    int 0x80

program_end:


