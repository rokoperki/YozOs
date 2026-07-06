;INT 0x15, eax=0xE820 Bious function to get memmory map 
;input: es:di -> destination of 24B entries 
;output: bp = entry count, trashes all registers except esi 
mmap_seg equ 0x7000
mmap_ent equ 0x0000 ; number of entries will be stored at 0x70000
do_e820:
  push es
  mov ax, mmap_seg
  mov es, ax
  mov di, 0x0004
  xor ebx, ebx      ;ebx must be 0 
  xor bp, bp        ;keep an entry count in bp 
  mov edx, 0x0534D4150    ;place "SMAP"
  mov eax, 0xe820 
  mov [es:di + 20], dword 1   ;force a valid ACPI 3.X entry
  mov ecx, 24     ;ask for 24B
  int 0x15 
  jc short .failed ;carry set on fist call means "unsupported function"
  mov edx, 0x0534D4150        ;on success, eax reset to "SMAP"
  cmp eax, edx 
  jne short .failed 
  test ebx, ebx     ;ebx=0 implies list is only 1 entry long 
  je short .failed 
  jmp short .jmpin 

.e820lp:
  mov eax, 0xe820   ;eax, ecx get trashen after int 0x15 call 
  mov [es:di + 20], dword 1 ;force a valid ACPI 3.X entry
  mov ecx, 24   ;ask for 24B
  int 0x15 
  jc short .e820f   ;carry set means "end of list already reached"
  mov edx, 0x0534D4150 
.jmpin:
  jcxz .skipent   ;skip any 0 lenght entries 
  cmp cl, 20      ;got a 24 byte ACPI 3.X response? 
  jbe short .notext
  test byte [es:di + 20], 1   ;if so: is the "ignore this data" bit clear?
  je short .skipent
.notext:
  mov ecx, [es:di + 8]    ;get lower uint32_t of mem region lenght 
  or ecx, [es:di + 12]    ; or it with upper uint32_t to test for zero 
  jz .skipent     ;if lenght of uint64_t is 0, skip etry 
  inc bp    ;good entri: increment count 
  add di, 24 
.skipent:
  test ebx, ebx     ;if ebs resets to 0, list is complete 
  jne short .e820lp
.e820f:
  mov [es:mmap_ent], bp    ;store entry count 
  clc   ;there is js on end of list to this point, so the carry must be cleared 
  pop es
  ret
.failed:
  pop es
  stc   ;function unsupported err exit 
  ret 
