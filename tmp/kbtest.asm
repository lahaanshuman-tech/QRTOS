
[BITS 16]
[ORG 0x7C00]
    mov ax, 0x0003
    int 0x10
    mov si, msg
    call print
loop:
    mov ah, 0x00
    int 0x16
    mov ah, 0x0E
    int 0x10
    jmp loop
print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print
.done:
    ret
msg db 'Type anything: ', 0
times 510-($-$$) db 0
dw 0xAA55

