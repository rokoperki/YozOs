;simple boot that demonstrates segnment offseting

mov ah, 0x0e ; scrolling teletype BIOS routine 

mov al, [the_secret]
int 0x10

mov bx, 0x7c0 ; set data segment register to 0x7c0 
mov ds, bx 
mov al, [the_secret]
int 0x10

mov al, [es:the_secret]
int 0x10

mov bx, 0x7c0 ; set general purpose segment register address 
mov es, bx 
mov al, [es:the_secret]
int 0x10

jmp $

the_secret:
  db "X"

;padding and BIOS num 
times 510-($-$$) db 0 
dw 0xaa55
