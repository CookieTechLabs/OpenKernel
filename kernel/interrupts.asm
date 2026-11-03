BITS 32

section .text

extern interrupt_handler

global isr_stub_table
global irq_stub_table
global syscall_stub

isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_handler
    add esp, 4
    mov esp, eax

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iretd

irq_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_handler
    add esp, 4
    mov esp, eax

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iretd

syscall_stub:
    push dword 0
    push dword 0x80
    jmp isr_common_stub

%macro ISR_NO_ERROR 1
    global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERROR 1
    global isr%1
isr%1:
    push dword %1
    jmp isr_common_stub
%endmacro

%macro IRQ 1
    global irq%1
irq%1:
    push dword 0
    push dword (32 + %1)
    jmp irq_common_stub
%endmacro

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR    8
ISR_NO_ERROR 9
ISR_ERROR    10
ISR_ERROR    11
ISR_ERROR    12
ISR_ERROR    13
ISR_ERROR    14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR    17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_ERROR    21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_ERROR    29
ISR_ERROR    30
ISR_NO_ERROR 31

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

section .rodata
align 4
isr_stub_table:
%assign vector 0
%rep 32
    dd isr%+vector
%assign vector vector + 1
%endrep

irq_stub_table:
%assign irq_number 0
%rep 16
    dd irq%+irq_number
%assign irq_number irq_number + 1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
