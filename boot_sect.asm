;simple boot that demonstrates conditions 
; checks conditions and prints char 
mov bx, 3

cmp bx, 4
jle less_equal
cmp bx, 40
jl less
mov al, "C"
jmp end 

less_equal:
  mov al, "A"
  jmp end
less:
  mov al, "B"

end:
  mov ah, 0x0e
  int 0x10
  jmp $

  times 510-($-$$) db 0 
  dw 0xaa55 
