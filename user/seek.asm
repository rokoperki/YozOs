bits 32
org 0x70000

SYS_EXIT equ 2
SYS_OPEN equ 11
SYS_READ equ 12
SYS_CLOSE equ 13
SYS_WRITE equ 14
SYS_LSEEK equ 16

USER_O_RDONLY equ 0
USER_SEEK_SET equ 0

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_OPEN
    mov ebx, path
    mov ecx, USER_O_RDONLY
    xor edx, edx
    int 0x80

    cmp eax, 0
    jl open_failed

    mov [fd], eax

    mov eax, SYS_LSEEK
    mov ebx, [fd]
    mov ecx, 6
    mov edx, USER_SEEK_SET
    int 0x80

    cmp eax, 0
    jl seek_failed

.read_loop:
    mov eax, SYS_READ
    mov ebx, [fd]
    mov ecx, buffer
    mov edx, buffer_len
    int 0x80

    cmp eax, 0
    jl read_failed

    cmp eax, 0
    je close_and_exit

    mov [bytes_read], eax

    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, buffer
    mov edx, [bytes_read]
    int 0x80

    jmp .read_loop

close_and_exit:
    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
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

seek_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, seek_msg
    mov edx, seek_msg_len
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

read_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, read_msg
    mov edx, read_msg_len
    int 0x80

    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, 3
    xor ecx, ecx
    xor edx, edx
    int 0x80

halt:
    jmp halt

path:
    db "OUT.TXT", 0

open_msg:
    db "open OUT.TXT failed", 10
open_msg_len equ $ - open_msg

seek_msg:
    db "seek OUT.TXT failed", 10
seek_msg_len equ $ - seek_msg

read_msg:
    db "read OUT.TXT failed", 10
read_msg_len equ $ - read_msg

fd:
    dd 0
bytes_read:
    dd 0
buffer:
    times 64 db 0
buffer_len equ 64

program_end:
