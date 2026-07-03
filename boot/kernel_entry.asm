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

cld ;ensure stosb counts upwards 
mov edi, __bss_start 
mov ecx, _end 
sub ecx, edi    ; ecx = num of bytes to zero 
xor eax, eax ; write 0 to [edi], ecx times 
rep stosb

call main           ; enter the C kernel
jmp $               ; if main ever returns, hang here instead of running garbage
