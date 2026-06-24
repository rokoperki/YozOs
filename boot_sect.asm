;simple boot that demonstrates segnment offseting
[org 0x7c00]

mov [BOOT_DRIVE], dl ; BIOS stores our boot drive in DL

mov bp, 0x8000  ; setup stack 
mov sp, bp

mov bx, 0x9000  ; load 5 sectors from es 0x0000 to bx 0x9000 
mov dh, 2       ; from boot disk 
mov dl, [BOOT_DRIVE]
call disk_load

mov dx, [0x9000]
call print_hex

mov bx, TAB
call print_string 

mov dx, [0x9000 + 512]
call print_hex

jmp $

%include "print_hex.asm"
%include "print_string.asm"
%include "disk_load.asm"

BOOT_DRIVE: db 0
TAB: db "  -  ", 0 

times 510-($-$$) db 0
dw 0xaa55

; data seed to read 
times 256 dw 0xdada
times 256 dw 0xface
