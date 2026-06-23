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
