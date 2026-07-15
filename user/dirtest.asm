bits 32
org 0x70000

SYS_EXIT equ 2
SYS_OPEN equ 11
SYS_READ equ 12
SYS_CLOSE equ 13
SYS_WRITE equ 14
SYS_STAT equ 15
SYS_LSEEK equ 16

USER_O_RDWR equ 2
USER_O_CREATE equ 4
USER_O_TRUNC equ 8
WRITE_FLAGS equ USER_O_RDWR | USER_O_CREATE | USER_O_TRUNC

USER_SEEK_SET equ 0

USER_STAT_TYPE_FILE equ 1
USER_STAT_TYPE_DIR equ 2

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_STAT
    mov ebx, dir_path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae dir_stat_failed

    cmp dword [stat_type], USER_STAT_TYPE_DIR
    jne dir_type_failed

    mov eax, SYS_OPEN
    mov ebx, file_path
    mov ecx, WRITE_FLAGS
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae open_failed

    mov [fd], eax

    mov eax, SYS_WRITE
    mov ebx, [fd]
    mov ecx, payload
    mov edx, payload_len
    int 0x80

    cmp eax, 0xFFFFFF00
    jae write_failed

    cmp eax, payload_len
    jne write_short

    mov eax, SYS_LSEEK
    mov ebx, [fd]
    xor ecx, ecx
    mov edx, USER_SEEK_SET
    int 0x80

    cmp eax, 0xFFFFFF00
    jae seek_failed

    cmp eax, 0
    jne seek_failed

    mov eax, SYS_READ
    mov ebx, [fd]
    mov ecx, read_buf
    mov edx, payload_len
    int 0x80

    cmp eax, 0xFFFFFF00
    jae read_failed

    cmp eax, payload_len
    jne read_short

    mov esi, payload
    mov edi, read_buf
    mov ecx, payload_len

compare_loop:
    cmp ecx, 0
    je compare_done

    mov al, [esi]
    cmp al, [edi]
    jne compare_failed

    inc esi
    inc edi
    dec ecx
    jmp compare_loop

compare_done:
    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov dword [fd], 0xFFFFFFFF

    mov eax, SYS_STAT
    mov ebx, file_path
    mov ecx, stat_buf
    xor edx, edx
    int 0x80

    cmp eax, 0xFFFFFF00
    jae file_stat_failed

    cmp dword [stat_type], USER_STAT_TYPE_FILE
    jne file_type_failed

    cmp dword [stat_size], payload_len
    jne file_size_failed

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

dir_stat_failed:
    mov ecx, dir_stat_msg
    mov edx, dir_stat_msg_len
    jmp fail_no_close

dir_type_failed:
    mov ecx, dir_type_msg
    mov edx, dir_type_msg_len
    jmp fail_no_close

open_failed:
    mov ecx, open_msg
    mov edx, open_msg_len
    jmp fail_no_close

write_failed:
    mov ecx, write_msg
    mov edx, write_msg_len
    jmp fail_close

write_short:
    mov ecx, write_short_msg
    mov edx, write_short_msg_len
    jmp fail_close

seek_failed:
    mov ecx, seek_msg
    mov edx, seek_msg_len
    jmp fail_close

read_failed:
    mov ecx, read_msg
    mov edx, read_msg_len
    jmp fail_close

read_short:
    mov ecx, read_short_msg
    mov edx, read_short_msg_len
    jmp fail_close

compare_failed:
    mov ecx, compare_msg
    mov edx, compare_msg_len
    jmp fail_close

file_stat_failed:
    mov ecx, file_stat_msg
    mov edx, file_stat_msg_len
    jmp fail_no_close

file_type_failed:
    mov ecx, file_type_msg
    mov edx, file_type_msg_len
    jmp fail_no_close

file_size_failed:
    mov ecx, file_size_msg
    mov edx, file_size_msg_len
    jmp fail_no_close

fail_close:
    push ecx
    push edx

    mov eax, SYS_CLOSE
    mov ebx, [fd]
    xor ecx, ecx
    xor edx, edx
    int 0x80

    pop edx
    pop ecx

fail_no_close:
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

dir_path:
    db "TESTDIR", 0

file_path:
    db "TESTDIR/A.TXT", 0

payload:
    db "subdir-ok"
payload_len equ $ - payload

ok_msg:
    db "dirtest ok", 10
ok_msg_len equ $ - ok_msg

dir_stat_msg:
    db "stat TESTDIR failed", 10
dir_stat_msg_len equ $ - dir_stat_msg

dir_type_msg:
    db "TESTDIR type failed", 10
dir_type_msg_len equ $ - dir_type_msg

open_msg:
    db "open TESTDIR/A.TXT failed", 10
open_msg_len equ $ - open_msg

write_msg:
    db "write TESTDIR/A.TXT failed", 10
write_msg_len equ $ - write_msg

write_short_msg:
    db "write TESTDIR/A.TXT short", 10
write_short_msg_len equ $ - write_short_msg

seek_msg:
    db "seek TESTDIR/A.TXT failed", 10
seek_msg_len equ $ - seek_msg

read_msg:
    db "read TESTDIR/A.TXT failed", 10
read_msg_len equ $ - read_msg

read_short_msg:
    db "read TESTDIR/A.TXT short", 10
read_short_msg_len equ $ - read_short_msg

compare_msg:
    db "read TESTDIR/A.TXT mismatch", 10
compare_msg_len equ $ - compare_msg

file_stat_msg:
    db "stat TESTDIR/A.TXT failed", 10
file_stat_msg_len equ $ - file_stat_msg

file_type_msg:
    db "TESTDIR/A.TXT type failed", 10
file_type_msg_len equ $ - file_type_msg

file_size_msg:
    db "TESTDIR/A.TXT size failed", 10
file_size_msg_len equ $ - file_size_msg

fd:
    dd 0xFFFFFFFF

stat_buf:
stat_size:
    dd 0
stat_type:
    dd 0

read_buf:
    times payload_len db 0

program_end:
