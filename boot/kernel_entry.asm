; kernel_entry — the first thing at 0x10000.
;
; The boot sector blindly `call 0x10000`. gcc does NOT guarantee main() is the
; first byte of kernel.o, so we link THIS stub first: it is guaranteed to sit at
; 0x10000, and it simply calls main() — whose real address the linker fills in.
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

; base=0x0 , limit=0xfffff ,
; 1st flags: (present)1 (privilege)11 (descriptor type)1 -> 1111b
; type flags: (code)1 (conforming)0 (readable)1 (accessed)0 -> 1010b
; 2nd flags: (granularity)1 (32 - bit default)1 (64 - bit seg)0 (AVL)0 -> 1100b
kernel_gdt_user_code:
    dw 0xffff ;limit bits 0-15
    dw 0x0 ;base bits 0-15 
    db 0x0 ;base bits 16-23     
    db 11111010b    ; 1st flags , type flags
    db 11001111b    ; 2nd flags , Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)


kernel_gdt_user_data:
    dw 0xffff ;limit bits 0-15
    dw 0x0  ;base bits 0-15
    db 0x0  ;base bits 16-23
    db 11110010b    ; 1st flags , type flags
    db 11001111b    ; 2nd flags , Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)

kernel_gdt_end:

kernel_gdt_descriptor:
  dw kernel_gdt_end - kernel_gdt_start - 1
  dd kernel_gdt_start

KERNEL_CODE_SEG equ kernel_gdt_code - kernel_gdt_start
KERNEL_DATA_SEG equ kernel_gdt_data - kernel_gdt_start
USER_CODE_SEG equ kernel_gdt_user_code - kernel_gdt_start 
USER_DATA_SEG equ kernel_gdt_user_data - kernel_gdt_start
