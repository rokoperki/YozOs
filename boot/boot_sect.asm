; boot sector that enters 32 bit mode 
[org 0x7c00]

mov bp, 0x9000     ;set the stack 
mov sp, bp 

mov bx, MSG_REAL_MODE
call print_string 

call switch_to_pm

jmp $

%include "print_string.asm"
%include "gdt.asm"
%include "print_string_pm.asm"
%include "pm_switch.asm"

[bits 32]

;This is where we arrive after switching to and initialising protected mode.
BEGIN_PM:
  mov ebx, MSG_PROT_MODE
  call print_string_pm
  
jmp $

; Global variables
MSG_REAL_MODE db "started in 16 - bit Real Mode ",0
MSG_PROT_MODE db "Successfully landed in 32 - bit Protected Mode ",0

; Bootsector padding
times 510-($-$$) db 0
dw 0xaa55
