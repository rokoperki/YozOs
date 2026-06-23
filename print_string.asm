print_string:
  mov ah, 0x0e
  start:
  mov al, [bx] ; loads value of b reg 
  cmp al, 0 ; if not 0 print string 
  je end
  int 0x10 ; print 
  add bx, 0x01 ; move address by 1
  jmp start
  end:
    ret

print_hex:
  mov cx, 0 ; digit counter 

  start_hex:
  mov ax, dx
  and ax, 0x000f
  add al, '0'        ; assume it's a digit
  cmp al, '9'        ; did we overshoot past '9'
  jle done           ; no -> it was 0..9, leave it
  add al, 7          ; yes -> it was A..F, jump the gap
  done:
    mov bx, HEX_OUT + 5 ; load last digit address  
    sub bx, cx ; sub current digit 
    mov [bx], al ; save parsed dx digit 
  add cx, 1  ;increment 
  shr dx, 4  ;shift dx for byte 0x1fb6 -> 0x01fb 
  cmp cx, 4  ;jumo if last digit done
  jl start_hex


    mov bx, HEX_OUT
    call print_string 
    ret
