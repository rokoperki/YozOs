; load DH sectors to ES:BX from drive DL 

disk_load:
  push dx ;push dx to stack to track how many sectors were requested to read 

  mov ah, 0x02 ; BIOS read sector function  
  mov al, dh ; read DH sectors
  mov ch, 0x00 ; cylinder 0 
  mov dh, 0x00 ; head 0 
  mov cl, 0x02 ; second sector (after boot sector)

  int 0x13 ; BIOS interupt 

  jc disk_error ; jump if carry flag set 

  pop dx  ;restore dx from stack 
  cmp dh, al  ; if sectors_read != sectors_expecter 
  jne disk_error
  ret 

disk_error:
  mov bx, DISK_ERR_MSG 
  call print_string
  jmp $

DISK_ERR_MSG: db "Disk read error! ",0 
