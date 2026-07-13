bits 32
org 0x70000

SYS_EXIT equ 2
SYS_OPEN equ 11
SYS_CLOSE equ 13
SYS_WRITE equ 14

USER_O_WRONLY equ 1
USER_O_CREATE equ 4
USER_O_APPEND equ 16
APPEND_FLAGS equ USER_O_WRONLY | USER_O_CREATE | USER_O_APPEND

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_OPEN
    mov ebx, path
    mov ecx, APPEND_FLAGS
    xor edx, edx
    int 0x80

    cmp eax, 0
    jl open_failed

    mov [fd], eax

    mov eax, SYS_WRITE
    mov ebx, [fd]
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    cmp eax, 0
    jl write_failed

    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

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

open_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, open_msg
    mov edx, open_msg_len
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, 1
    xor ecx, ecx
    xor edx, edx
    int 0x80

write_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, write_msg
    mov edx, write_msg_len
    int 0x80

    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, 2
    xor ecx, ecx
    xor edx, edx
    int 0x80

halt:
    jmp halt

path:
    db "OUT.TXT", 0

msg:
    db "appended from user", 10
msg_len equ $ - msg

ok_msg:
    db "append ok", 10
ok_msg_len equ $ - ok_msg

open_msg:
    db "open OUT.TXT failed", 10
open_msg_len equ $ - open_msg

write_msg:
    db "append OUT.TXT failed", 10
write_msg_len equ $ - write_msg

fd:
    dd 0

program_end:
