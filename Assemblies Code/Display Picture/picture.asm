BITS 16
org 0x7c00

segments_clear:
    cli
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, 0x7c00
	mov bp, sp
	sti
	
start:
   mov ax, 0x13
   int 0x10
   
read_disk:
   mov ah, 0x42
   mov si, bootdap
   int 0x13
   
   jmp $
   
bootdap:
   db 0x10
   db 0
   dw (filled - image) / 512 + 1
   dw 0x0000, 0xA000
   dq 1
   
boot_error:
   mov ah, 0x0e
   mov al, "x"
   int 0x10
   jmp stop
   
stop:
  hlt
   
times 510 - ($-$$) db 0
dw 0xAA55

image: incbin "data.vad"
filled: times 512 - ($-$$) % 512 db 0
