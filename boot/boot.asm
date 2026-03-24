[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov ax, 0x0003
    int 0x10

    mov si, msg_load
    call print16

    xor ah, ah
    xor dl, dl
    int 0x13

    mov ah, 0x02
    mov al, 17
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x00
    mov bx, 0x1000
    int 0x13
    jc disk_error

    mov ah, 0x02
    mov al, 18
    mov ch, 1
    mov cl, 1
    mov dh, 0
    mov dl, 0x00
    mov bx, 0x3200
    int 0x13
    jc disk_error2

    mov si, msg_ok
    call print16

    cli

    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp 0x08:init_pm

disk_error:
    mov si, msg_err
    call print16
    cli
    hlt

disk_error2:
    mov si, msg_err2
    call print16
    cli
    hlt

print16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print16
.done:
    ret

align 8
gdt_start:
gdt_null:
    dq 0x0000000000000000
gdt_code32:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data32:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32]
init_pm:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x7FF00

    ; Remap PIC
    mov al, 0x11
    out 0x20, al
    out 0xA0, al
    mov al, 0x20
    out 0x21, al
    mov al, 0x28
    out 0xA1, al
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    mov al, 0x01
    out 0x21, al
    out 0xA1, al
    mov al, 0xFD
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al

    call 0x1000
    cli
    hlt

msg_load  db 'QRTOS Bootloader v1 - Loading kernel...', 13, 10, 0
msg_ok    db 'Kernel loaded! Starting QRTOS...', 13, 10, 0
msg_err   db 'ERROR: First read failed!', 13, 10, 0
msg_err2  db 'ERROR: Second read failed!', 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55
