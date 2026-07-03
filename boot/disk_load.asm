; load DH sectors to ES:BX from drive DL 

disk_load:
  pusha 
  mov [count], dh      ; save sector  

  mov word [lba], 1   ;LBA 1 start (0 - boot sect)
  
  .loop: 
  ; conver LBA to CHS 
  mov ax, [lba] 
  xor dx, dx 
  mov si, 18 
  div si ; ax = LBA/18 (track), dx = lba%18 (sector offset)

  inc dx
  mov [sect], dl    ;stash sector 

  xor dx, dx 
  mov si, 2
  div si  ; ax = cylinder, dx = head 
  mov [head], dl ;stash head 

  ;read one sector 
  mov ah, 0x02 
  mov al, 1 ;one sector 
  mov ch, 0 ;cylinder low byte 
  mov cl, [sect] 
  mov dh, [head]
  mov dl, [BOOT_DRIVE]
  int 0x13
  jc disk_error 

  ;advance to next sector 
  add bx, 512 
  inc word [lba]
  dec byte [count]
  jnz .loop 
  
  popa 
  ret 
  
disk_error:
  mov bx, DISK_ERR_MSG 
  call print_string
  jmp $

DISK_ERR_MSG: db "Disk read error! ",0 

lba: dw 0 
sect: db 0 
head: db 0 
count: db 0
