[bits 16]
;switch to protected mode 
switch_to_pm:
  cli       ; switch off interupts 

  lgdt [gdt_descriptor]   ;load GTD 

  mov eax, cr0 
  or eax, 0x1
  mov cr0, eax      ; make a switch, updated cr0 CPU control register 

  jmp CODE_SEG:init_pm    ;large jumpt to 32-bit code in new mem segment
  ;forces CPU to flush its cache of pre-fetched real mode decoded instructions

[bits 32]
init_pm:
  mov ax, DATA_SEG    ;point segment registers to new data segment
  mov ds , ax
  mov ss , ax
  mov es , ax
  mov fs , ax
  mov gs , ax

  mov ebp, 0x90000    ;update stack position 
  mov esp, ebp 

  call BEGIN_PM
