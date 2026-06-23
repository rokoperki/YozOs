;simple boot that demonstrates the stack  

mov ah, 0x0e    ;int 10/ah = 0eh -> scrolling BIOS routine 

mov bp, 0x8000 ; set base of stack little above boot sector 
mov sp, bp 

push 'A' ; push characters to stack, pushed as 16-bit values
push 'B'
push 'C'

pop bx  ; pop to register b, then copy bl (8bit char)
mov al, bl 
int 0x10

pop cx ; same as above but in reg cx 
mov al, cl 
int 0x10

mov al, [0x7ffe] ; print first char at stack 0x8000 - 0x2 (C now i not pop A)

int 0x10 

jmp $

times 510-($-$$) db 0 
dw 0xaa55 
