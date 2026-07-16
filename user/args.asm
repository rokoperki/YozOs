bits 32
global _start

%define SYS_EXIT 2
%define SYS_WRITE 14
%define STDOUT 1

section .text
_start:
  mov esi, esp

  mov eax, [esi]
  mov [argc_value], eax

  mov eax, msg_argc
  call write_cstr

  mov eax, [argc_value]
  call write_dec

  mov eax, newline
  call write_cstr

  mov edi, [esi + 4]
  xor ebp, ebp

.print_loop:
  cmp ebp, [argc_value]
  jae .done

  mov eax, msg_argv
  call write_cstr

  mov eax, ebp
  call write_dec

  mov eax, msg_close
  call write_cstr

  mov eax, [edi + ebp * 4]
  call write_cstr

  mov eax, newline
  call write_cstr

  inc ebp
  jmp .print_loop

.done:
  mov eax, SYS_EXIT
  xor ebx, ebx
  int 0x80

write_cstr:
  push eax
  mov ecx, eax
  xor edx, edx

.len_loop:
  cmp byte [ecx + edx], 0
  je .len_done
  inc edx
  jmp .len_loop

.len_done:
  pop ecx
  mov eax, SYS_WRITE
  mov ebx, STDOUT
  int 0x80
  ret

write_dec:
  push ebx
  push ecx
  push edx
  push edi

  mov edi, dec_buf + 15
  mov byte [edi], 0
  mov ebx, 10

  cmp eax, 0
  jne .digits

  dec edi
  mov byte [edi], '0'
  jmp .emit

.digits:
  xor edx, edx
  div ebx
  add dl, '0'
  dec edi
  mov [edi], dl
  cmp eax, 0
  jne .digits

.emit:
  mov eax, edi
  call write_cstr

  pop edi
  pop edx
  pop ecx
  pop ebx
  ret

section .data
argc_value dd 0
msg_argc db "argc=", 0
msg_argv db "argv[", 0
msg_close db "]=", 0
newline db 10, 0
dec_buf times 16 db 0
