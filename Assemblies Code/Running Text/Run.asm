org 7C00h
bits 16

VIDEO_MEMORY equ 0B800h
ATTRIBUTE    equ 70h

start:

    cli
    xor ax,ax
    mov ds,ax
    sti

    mov ax,0003h
    int 10h

    mov word [Position],80

MainLoop:

    call DrawFrame
    call Delay

    dec word [Position]

    ; Restart once the text disappears
    mov ax,[Position]
    cmp ax,-60
    jge MainLoop

    mov word [Position],80
    jmp MainLoop

DrawFrame:

    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov ax,VIDEO_MEMORY
    mov es,ax
    xor di,di
    mov cx,80

FillRow:

    mov ax,(ATTRIBUTE << 8) | ' '
    stosw
    loop FillRow
    mov si,Message
    mov bx,[Position]

NextChar:

    lodsb

    cmp al,0
    je Finished

    cmp bx,0
    jl SkipChar

    cmp bx,79
    jg SkipChar

    mov di,bx
    shl di,1

    mov ah,ATTRIBUTE
    stosw

SkipChar:

    inc bx
    jmp NextChar

Finished:

    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

Delay:

    push cx
    push dx

    mov cx,5020
; Main Delay
.outer:
    mov dx,65535
; Second Delay
.inner:
    dec dx
    jnz .inner

    loop .outer

    pop dx
    pop cx
    ret


Position dw 80
Message db " If You Can See This The Code is Working Fine",0
times 510-($-$$) db 0
dw 0AA55h
