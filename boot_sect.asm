;simple boot that demonstrates addressing
[org 0x7c00] ; expected memmory location

mov ah, 0x0e    ;int 10/ah = 0eh -> scrolling BIOS routine 

;first attempt
mov al, the_secret  ; moves offset to print as character - wont work 
int 0x10

;second attempt
mov al, [the_secret] ; moves char at that address to print - works if [org 0x7c00] is set 
int 0x10

;third attempt
mov bx, the_secret  ; works if org isnt set, if yes prints " "
add bx, 0x7c00
mov al, [bx]
int 0x10

;fourt attempt
mov al, [0x7c1d] ; works if bytecode isnt changed 
int 0x10

jmp $

the_secret:
  db "X"

times 510-($-$$) db 0 
dw 0xaa55
