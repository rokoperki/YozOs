bits 32
org 0x70000

SYS_EXIT equ 2
SYS_OPEN equ 11
SYS_READ equ 12
SYS_CLOSE equ 13
SYS_WRITE equ 14
SYS_LSEEK equ 16

USER_O_RDWR equ 2
USER_O_CREATE equ 4
USER_O_TRUNC equ 8
WRITE_FLAGS equ USER_O_RDWR | USER_O_CREATE | USER_O_TRUNC

USER_SEEK_SET equ 0

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_OPEN
    mov ebx, path
    mov ecx, WRITE_FLAGS
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae open_failed

    mov [fd], eax

    mov eax, SYS_WRITE
    mov ebx, [fd]
    mov ecx, initial_msg
    mov edx, initial_msg_len
    int 0x80

    cmp eax, 0xFFFFFF00
    jae write_failed

    mov eax, SYS_LSEEK
    mov ebx, [fd]
    mov ecx, 2
    mov edx, USER_SEEK_SET
    int 0x80

    cmp eax, 0xFFFFFF00
    jae seek_failed

    mov eax, SYS_WRITE
    mov ebx, [fd]
    mov ecx, patch_msg
    mov edx, patch_msg_len
    int 0x80

    cmp eax, 0xFFFFFF00
    jae write_failed

    mov eax, SYS_LSEEK
    mov ebx, [fd]
    xor ecx, ecx
    mov edx, USER_SEEK_SET
    int 0x80

    cmp eax, 0xFFFFFF00
    jae seek_failed

    mov eax, SYS_READ
    mov ebx, [fd]
    mov ecx, buffer
    mov edx, buffer_len
    int 0x80

    cmp eax, 0xFFFFFF00
    jae read_failed

    mov [bytes_read], eax

    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, buffer
    mov edx, [bytes_read]
    int 0x80

    cmp eax, 0xFFFFFF00
    jae write_failed

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

    jmp close_error_exit

read_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, read_msg
    mov edx, read_msg_len
    int 0x80

    jmp close_error_exit

write_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, write_msg
    mov edx, write_msg_len
    int 0x80

close_error_exit:
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

initial_msg:
    db "abcdef", 10
initial_msg_len equ $ - initial_msg

patch_msg:
    db "ZZ"
patch_msg_len equ $ - patch_msg

open_msg:
    db "open OUT.TXT failed", 10
open_msg_len equ $ - open_msg

seek_msg:
    db "seek OUT.TXT failed", 10
seek_msg_len equ $ - seek_msg

read_msg:
    db "read OUT.TXT failed", 10
read_msg_len equ $ - read_msg

write_msg:
    db "write OUT.TXT failed", 10
write_msg_len equ $ - write_msg

fd:
    dd 0
bytes_read:
    dd 0
buffer:
    times 32 db 0
buffer_len equ 32

program_end:
