[bits 32]
global enter_user_mode

enter_user_mode:
  mov esi, [esp + 4]  ;entry 
  mov edi, [esp + 8]  ;user_stack 

  mov ax, 0x23 
  mov ds, ax 
  mov es, ax 
  mov fs, ax 
  mov gs, ax 

  push 0x23 ;user SS  
  push edi ;user ESP 

  pushfd 
  pop eax 
  or eax, 0x200
  push eax  ; eflags 

  push 0x1B ;user CS 
  push esi  ;user EIP 
  iret
