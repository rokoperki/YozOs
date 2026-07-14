bits 32
org 0x70000

SYS_EXIT equ 2
SYS_WRITE equ 14
SYS_STAT equ 15

USER_STAT_TYPE_FILE equ 1
USER_STAT_TYPE_DIR equ 2

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_STAT
    mov ebx, root_path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae root_stat_failed

    cmp dword [stat_type], USER_STAT_TYPE_DIR
    jne root_type_failed

    mov eax, SYS_STAT
    mov ebx, dot_path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae dot_stat_failed

    cmp dword [stat_type], USER_STAT_TYPE_DIR
    jne dot_type_failed

    mov eax, SYS_STAT
    mov ebx, file_path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae file_stat_failed

    cmp dword [stat_type], USER_STAT_TYPE_FILE
    jne file_type_failed

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

root_stat_failed:
    mov ecx, root_stat_msg
    mov edx, root_stat_msg_len
    jmp print_fail

root_type_failed:
    mov ecx, root_type_msg
    mov edx, root_type_msg_len
    jmp print_fail

dot_stat_failed:
    mov ecx, dot_stat_msg
    mov edx, dot_stat_msg_len
    jmp print_fail

dot_type_failed:
    mov ecx, dot_type_msg
    mov edx, dot_type_msg_len
    jmp print_fail

file_stat_failed:
    mov ecx, file_stat_msg
    mov edx, file_stat_msg_len
    jmp print_fail

file_type_failed:
    mov ecx, file_type_msg
    mov edx, file_type_msg_len
    jmp print_fail

print_fail:
    mov eax, SYS_WRITE
    mov ebx, 2
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, 1
    xor ecx, ecx
    xor edx, edx
    int 0x80

halt:
    jmp halt

root_path:
    db "/", 0
dot_path:
    db ".", 0
file_path:
    db "OUT.TXT", 0

ok_msg:
    db "dirstat ok", 10
ok_msg_len equ $ - ok_msg

root_stat_msg:
    db "stat / failed", 10
root_stat_msg_len equ $ - root_stat_msg

root_type_msg:
    db "stat / type failed", 10
root_type_msg_len equ $ - root_type_msg

dot_stat_msg:
    db "stat . failed", 10
dot_stat_msg_len equ $ - dot_stat_msg

dot_type_msg:
    db "stat . type failed", 10
dot_type_msg_len equ $ - dot_type_msg

file_stat_msg:
    db "stat OUT.TXT failed", 10
file_stat_msg_len equ $ - file_stat_msg

file_type_msg:
    db "stat OUT.TXT type failed", 10
file_type_msg_len equ $ - file_type_msg

stat_buf:
stat_size:
    dd 0
stat_type:
    dd 0

program_end:
