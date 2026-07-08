bits 32
org 0x70000

SYS_EXIT equ 2
SYS_STRING_WRITE equ 3

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_STRING_WRITE
    mov ebx, msg
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_EXIT
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    int 0x80

halt:
    jmp halt

msg:
    db "hello from FAT user program", 10, 0

program_end:
