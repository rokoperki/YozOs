[bits 32]
global syscall_stub
extern syscall_handler

syscall_stub:
  pusha ;save user general regs 
  
  mov esi, esp

  mov ax, ds  ;save user data segment 
  push eax 

  mov ax, 0x10 
  mov ds, ax
  mov es, ax 
  mov fs, ax 
  mov gs, ax 

  push dword [esi + 20] ;arg3 
  push dword [esi + 24] ;arg2 
  push dword [esi + 16] ;arg1 
  push dword [esi + 28] ;num 

  call syscall_handler
  add esp, 16 

  mov [esp + 32], eax

  pop ebx 
  mov ds, bx 
  mov es, bx 
  mov fs, bx 
  mov gs, bx 

  popa 

  iret
