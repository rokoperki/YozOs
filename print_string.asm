[bits 32]

;define mem constants
VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

; prints a null - terminated string pointed to by EDX
print_string_pm:
  push a 
  mov edx, VIDEO_MEMORY   ; set edx to start of VGA

print_string_pm_loop:
  mov al, [ebx]     ; store char of ebx in AL 
  mov ah, WHITE_ON_BLACK      ;store attributes 

  cmp al, 0
  je print_string_pm_done     ;if end of string finish printing 

  mov [edx], ax     ;store current char on cell 

  add ebx, 1    ;move to next char 
  add edx, 2    ;move to next char cell 

  jmp print_string_pm_loop

print_string_pm_done:
  pop a 
  ret
  

