#ifndef KERNEL_INTERRUPTS_H
#define KERNEL_INTERRUPTS_H

#include <stdint.h>

void interrupts_init(void);
void interrupts_enable_irq(uint8_t irq);
void interrupts_enable(void);
uint32_t *interrupt_handler(void *registers);

#endif
