;simple boot sector that prints message using BUIS routine


mov ah, 0x0e  ; int 10 -> scrolling teletype BIOS routine

mov al, 'H'
int 0x10
mov al, 'e'
int 0x10
mov al, 'l'
int 0x10
mov al, 'l'
int 0x10
mov al, 'o'
int 0x10

jmp $ ; infinite loop

;padding and BIOS num 

times 510-($-$$) db 0 

dw 0xaa55
