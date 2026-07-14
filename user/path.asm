bits 32
org 0x70000

SYS_EXIT equ 2
SYS_OPEN equ 11
SYS_CLOSE equ 13
SYS_WRITE equ 14
SYS_STAT equ 15

USER_O_RDONLY equ 0

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_OPEN
    mov ebx, abs_path
    mov ecx, USER_O_RDONLY
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae abs_open_failed

    mov [fd], eax

    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_STAT
    mov ebx, dot_path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae dot_stat_failed

    mov eax, SYS_OPEN
    mov ebx, nested_path
    mov ecx, USER_O_RDONLY
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jb nested_open_unexpected

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

abs_open_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, abs_msg
    mov edx, abs_msg_len
    int 0x80

    jmp fail_exit

dot_stat_failed:
    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, dot_msg
    mov edx, dot_msg_len
    int 0x80

    jmp fail_exit

nested_open_unexpected:
    mov [fd], eax

    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, 2
    mov ecx, nested_msg
    mov edx, nested_msg_len
    int 0x80

fail_exit:
    mov eax, SYS_EXIT
    mov ebx, 1
    xor ecx, ecx
    xor edx, edx
    int 0x80

halt:
    jmp halt

abs_path:
    db "/OUT.TXT", 0
dot_path:
    db "./OUT.TXT", 0
nested_path:
    db "DIR/OUT.TXT", 0

ok_msg:
    db "path ok", 10
ok_msg_len equ $ - ok_msg

abs_msg:
    db "absolute path failed", 10
abs_msg_len equ $ - abs_msg

dot_msg:
    db "dot path failed", 10
dot_msg_len equ $ - dot_msg

nested_msg:
    db "nested path unexpectedly opened", 10
nested_msg_len equ $ - nested_msg

fd:
    dd 0
stat_buf:
stat_size:
    dd 0
stat_type:
    dd 0

program_end:
