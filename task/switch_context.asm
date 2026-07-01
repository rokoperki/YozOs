[bits 32]

global switch_context
global task_trampoline

task_trampoline:
  sti 
  ret

switch_context:
  push ebp
  push ebx
  push esi
  push edi 

  mov eax, [esp + 20]   ; old_esp pointer (16B of pushes + 4ret)
  mov [eax], esp        ; *old_esp = esp -> park current task 

  mov eax, [esp + 24]   ; new_esp value 
  mov esp, eax 

  pop edi
  pop esi 
  pop ebx
  pop ebp
  ret
