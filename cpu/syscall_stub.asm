[bits 32]
global syscall_stub
extern syscall_handler

syscall_stub:
  push edx  ;arg3 
  push ecx  ;arg2 
  push ebx  ;arg1 
  push eax  ;num 

  call syscall_handler
  add esp, 16 

  iret
