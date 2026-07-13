  bits 32
  org 0x70000

  SYS_EXIT equ 2
  SYS_YIELD equ 6
  SYS_READ equ 12
  SYS_WRITE equ 14

      db "YOZ1"
      dd 0x70000
      dd start - $$
      dd program_end - $$

  start:
      mov eax, SYS_WRITE
      mov ebx, 1
      mov ecx, prompt
      mov edx, prompt_len
      int 0x80

  .wait_line:
      mov eax, SYS_YIELD
      xor ebx, ebx
      xor ecx, ecx
      xor edx, edx
      int 0x80

      mov eax, SYS_READ
      xor ebx, ebx          ; fd 0
      mov ecx, input_buf
      mov edx, input_buf_len
      int 0x80

      cmp eax, 0
      je .wait_line
      jl failed

      mov [bytes_read], eax

      mov eax, SYS_WRITE
      mov ebx, 1
      mov ecx, got_msg
      mov edx, got_msg_len
      int 0x80

      mov eax, SYS_WRITE
      mov ebx, 1
      mov ecx, input_buf
      mov edx, [bytes_read]
      int 0x80

      mov eax, SYS_WRITE
      mov ebx, 1
      mov ecx, newline
      mov edx, 1
      int 0x80

      mov eax, SYS_EXIT
      xor ebx, ebx
      xor ecx, ecx
      xor edx, edx
      int 0x80

  failed:
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

  prompt:
      db "type line: "
  prompt_len equ $ - prompt

  got_msg:
      db "you typed: "
  got_msg_len equ $ - got_msg

  newline:
      db 10

  fail_msg:
      db "read stdin failed", 10
  fail_msg_len equ $ - fail_msg

  bytes_read:
      dd 0

  input_buf:
      times 64 db 0
  input_buf_len equ 64

  program_end:
