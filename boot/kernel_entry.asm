; kernel_entry — the first thing at 0x1000.
;
; The boot sector blindly `call 0x1000`. gcc does NOT guarantee main() is the
; first byte of kernel.o, so we link THIS stub first: it is guaranteed to sit at
; 0x1000, and it simply calls main() — whose real address the linker fills in.
; Assembled as ELF (`nasm -f elf`) so `extern main` can be resolved at link time.

[bits 32]
[extern main]       ; main lives in kernel.o; linker substitutes its final address
[extern __bss_start] ;ld provided start and end of .bss 
[extern _end]

mov esp, 0x90000 
mov ebp, esp

cld ;ensure stosb counts upwards 
mov edi, __bss_start 
mov ecx, _end 
sub ecx, edi    ; ecx = num of bytes to zero 
xor eax, eax ; write 0 to [edi], ecx times 
rep stosb

lgdt [kernel_gdt_descriptor]
jmp KERNEL_CODE_SEG:.reload_segments

.reload_segments:
mov ax, KERNEL_DATA_SEG
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

call main           ; enter the C kernel
jmp $               ; if main ever returns, hang here instead of running garbage

kernel_gdt_start:
kernel_gdt_null:
  dd 0x0
  dd 0x0

kernel_gdt_code:
  dw 0xffff
  dw 0x0
  db 0x0
  db 10011010b
  db 11001111b
  db 0x0

kernel_gdt_data:
  dw 0xffff
  dw 0x0
  db 0x0
  db 10010010b
  db 11001111b
  db 0x0

kernel_gdt_end:

kernel_gdt_descriptor:
  dw kernel_gdt_end - kernel_gdt_start - 1
  dd kernel_gdt_start

KERNEL_CODE_SEG equ kernel_gdt_code - kernel_gdt_start
KERNEL_DATA_SEG equ kernel_gdt_data - kernel_gdt_start
