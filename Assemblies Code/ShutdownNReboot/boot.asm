[org 0x7c00]

start:
    mov ah, 0x53 
    mov al, 0x00
    xor bx, bx         
    int 0x15           
     
    mov ah, 0x53
    mov al, 0x04
    xor bx, bx
    int 0x15
    mov ah, 0x53
    mov al, 0x01
    xor bx, bx
    int 0x15
   
    mov ah, 0x53
    mov al, 0x08
    mov bx, 0x0001
    mov cx, 0x0001
    int 0x15
    

    mov ah, 0x53
    mov al, 0x07 
    mov bx, 0x0001
    mov cx, 0x0003
    int 0x15

apm_error:
    db 0x0ea
    dw 0x0000
    dw 0xFFFF
    PUSH    0FFFFh
    PUSH    0000h
    RET


times 510-($-$$) db 0  
dw 0xAA55              
