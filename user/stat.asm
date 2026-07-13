bits 32
org 0x70000

SYS_EXIT equ 2
SYS_WRITE equ 14
SYS_STAT equ 15

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_STAT
    mov ebx, path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0
    jl stat_failed

    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, ok_msg
    mov edx, ok_msg_len
    int 0x80

    mov eax, SYS_EXIT
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    int 0x80

stat_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, fail_msg
    mov edx, fail_msg_len
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, 1
    xor ecx, ecx
    xor edx, edx
    int 0x80

halt:
    jmp halt

path:
    db "TEST.TXT", 0

ok_msg:
    db "stat ok", 10
ok_msg_len equ $ - ok_msg

fail_msg:
    db "stat failed", 10
fail_msg_len equ $ - fail_msg

stat_buf:
stat_size:
    dd 0
stat_type:
    dd 0

program_end:
