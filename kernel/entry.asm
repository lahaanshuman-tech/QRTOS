[BITS 32]

extern kernel_main
extern irq1_handler
global _start
global idt_flush
global irq1_wrapper

_start:
    mov esp, 0x7FF00
    mov ebp, 0x7FF00
    call kernel_main
    cli
    hlt

idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

irq1_wrapper:
    pusha
    call irq1_handler
    popa
    iret
