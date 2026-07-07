[bits 32]

global user_context_save
global user_context_restore

saved_esp dd 0 
saved_ebp dd 0 
saved_eip dd 0 

user_context_save:
  mov eax, [esp]
  mov [saved_eip], eax 

  lea eax, [esp + 4]
  mov [saved_esp], eax 

  mov [saved_ebp], ebp 

  xor eax, eax 
  ret 


user_context_restore:
    mov ebx, [esp + 4]    ; restored return value

    mov ax, 0x10
    mov ds, ax 
    mov es, ax 
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, [saved_esp]
    mov ebp, [saved_ebp]

    mov eax, ebx
    jmp [saved_eip]
