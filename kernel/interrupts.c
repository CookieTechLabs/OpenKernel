#include <stdint.h>
#include <kernel.h>
#include <kernel/interrupts.h>
#include <kernel/ports.h>
#include <kernel/syscall.h>
#include <driver/mouse.h>
#include <driver/keyboard.h>
#include <kernel/process.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20
#define PIC1_OFFSET  0x20
#define PIC2_OFFSET  0x28
#define IDT_ENTRIES  129
#define SYSCALL_VECTOR 0x80

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t attributes;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern uintptr_t isr_stub_table[32];
extern uintptr_t irq_stub_table[16];
extern void syscall_stub(void);

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idtr;

static void io_wait(void) {
    port_byte_out(0x80, 0);
}

static void idt_set_gate(uint8_t vector, uintptr_t handler) {
    uint32_t address = (uint32_t)handler;

    idt[vector].offset_low = (uint16_t)(address & 0xFFFF);
    /* GRUB's Multiboot GDT uses 0x10 for 32-bit code and 0x18 for data. */
    idt[vector].selector = 0x10;
    idt[vector].zero = 0;
    idt[vector].attributes = vector == SYSCALL_VECTOR ? 0xEE : 0x8E;
    idt[vector].offset_high = (uint16_t)(address >> 16);
}

static void pic_remap(void) {
    port_byte_out(PIC1_COMMAND, 0x11);
    io_wait();
    port_byte_out(PIC2_COMMAND, 0x11);
    io_wait();
    port_byte_out(PIC1_DATA, PIC1_OFFSET);
    io_wait();
    port_byte_out(PIC2_DATA, PIC2_OFFSET);
    io_wait();
    port_byte_out(PIC1_DATA, 0x04);
    io_wait();
    port_byte_out(PIC2_DATA, 0x02);
    io_wait();
    port_byte_out(PIC1_DATA, 0x01);
    io_wait();
    port_byte_out(PIC2_DATA, 0x01);
    io_wait();

    /* Keep every IRQ masked until its device driver is initialized. */
    port_byte_out(PIC1_DATA, 0xFF);
    port_byte_out(PIC2_DATA, 0xFF);
}

void interrupts_init(void) {
    __asm__ volatile ("cli");

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_gate(vector, isr_stub_table[vector]);
    }
    for (uint8_t irq = 0; irq < 16; irq++) {
        idt_set_gate((uint8_t)(PIC1_OFFSET + irq), irq_stub_table[irq]);
    }
    idt_set_gate(SYSCALL_VECTOR, (uintptr_t)syscall_stub);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint32_t)(uintptr_t)&idt[0];
    __asm__ volatile ("lidt %0" : : "m"(idtr));

    pic_remap();
}

void interrupts_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;

    if (irq >= 16) {
        return;
    }
    if (irq < 8) {
        port = PIC1_DATA;
        mask = (uint8_t)(1u << irq);
        port_byte_out(port, (uint8_t)(port_byte_in(port) & (uint8_t)~mask));
        return;
    }

    port = PIC2_DATA;
    mask = (uint8_t)(1u << (irq - 8));
    port_byte_out(port, (uint8_t)(port_byte_in(port) & (uint8_t)~mask));
    port_byte_out(PIC1_DATA,
                  (uint8_t)(port_byte_in(PIC1_DATA) & (uint8_t)~(1u << 2)));
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

uint32_t *interrupt_handler(void *registers) {
    struct kernel_panic_registers *frame =
        (struct kernel_panic_registers *)registers;
    uint32_t vector = frame->vector;

    if (vector < 32) {
        char message[] = "CPU exception 00";
        message[14] = (char)('0' + (vector / 10));
        message[15] = (char)('0' + (vector % 10));
        panic_at(message, __FILE__, __LINE__, frame);
    }

    if (vector == SYSCALL_VECTOR) {
        uint32_t syscall_number = frame->eax;
        process_interrupt_enter((uint32_t *)frame);
        frame->eax = (uint32_t)syscall_dispatch(frame->eax, frame->ebx,
                                                frame->ecx, frame->edx);
        return process_interrupt_return((uint32_t *)frame, syscall_number);
    }

    if (vector >= PIC1_OFFSET && vector < PIC1_OFFSET + 16) {
        uint8_t irq = (uint8_t)(vector - PIC1_OFFSET);
        if (irq == 1) {
            keyboard_irq_handler();
        } else if (irq == 12) {
            mouse_irq_handler();
        }
        if (irq >= 8) {
            port_byte_out(PIC2_COMMAND, PIC_EOI);
        }
        port_byte_out(PIC1_COMMAND, PIC_EOI);
    }
    return (uint32_t *)frame;
}
