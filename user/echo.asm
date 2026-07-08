bits 32
org 0x70000

SYS_EXIT equ 2
SYS_STRING_WRITE equ 3
SYS_READ_LINE equ 5
SYS_YIELD equ 6

    db "YOZ1"
    dd 0x70000
    dd start - $$
    dd program_end - $$

start:
    mov eax, SYS_STRING_WRITE
    mov ebx, prompt
    xor ecx, ecx
    xor edx, edx
    int 0x80

.wait_line:
    mov eax, SYS_YIELD
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_READ_LINE
    mov ebx, input_buf
    mov ecx, input_buf_len
    xor edx, edx
    int 0x80

    test eax, eax
    jz .wait_line

    mov eax, SYS_STRING_WRITE
    mov ebx, got_msg
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_STRING_WRITE
    mov ebx, input_buf
    xor ecx, ecx
    xor edx, edx
    int 0x80

    mov eax, SYS_STRING_WRITE
    mov ebx, newline
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

prompt:
    db "type line: ", 0
got_msg:
    db "you typed: ", 0
newline:
    db 10, 0
input_buf:
    times 64 db 0
input_buf_len equ 64

program_end:
