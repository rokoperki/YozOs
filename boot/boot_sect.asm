; boot sector that boots C kernel i 32-bit protected mode 
[org 0x7c00]
KERNEL_OFFSET equ 0x1000    ; memory where we load kernel (and where kernel_entry sits)

xor ax, ax                  ; zero the segment registers first — BIOS doesn't
mov ds, ax                  ; guarantee they're 0 on real hardware, and the
mov es, ax                  ; disk read + string prints all assume segment 0.
mov ss, ax                  ; (xor ax leaves DL — the boot drive — untouched)

mov [BOOT_DRIVE], dl        ; BIOS stores our boot drive in DL

mov bp, 0x9000              ; setup stack
mov sp, bp

mov bx, MSG_REAL_MODE
call print_string           ; announce that 16-bit real mode boot 

call load_kernel
call do_e820
call switch_to_pm

jmp $ 

%include "memory_map.asm"
%include "print_string.asm"
%include "disk_load.asm"
%include "gdt.asm"
%include "print_string_pm.asm"
%include "pm_switch.asm"

[bits 16]

load_kernel:
  mov bx, MSG_LOAD_KERNEL    
  call print_string

  mov bx, KERNEL_OFFSET       ; set params of disk_load so that we load first 15 sectors 
  mov dh, 31                  ; excluding boot, from boot disk to address KERNEL_OFFSET
  mov dl, [BOOT_DRIVE]
  call disk_load

  ret 

[bits 32]

BEGIN_PM:

  mov ebx, MSG_PROT_MODE      ;announce protected mode
  call print_string_pm

  call KERNEL_OFFSET ; jump to 0x1000 — the kernel_entry stub, which calls main

  jmp $

; Global variables
BOOT_DRIVE db 0
MSG_REAL_MODE db "Started in 16 - bit Real Mode " , 0
MSG_PROT_MODE db "Successfully landed in 32 - bit Protected Mode " , 0
MSG_LOAD_KERNEL db "Loading kernel into memory. " , 0

; Bootsector padding
times 510-($-$$) db 0
dw 0xaa55
