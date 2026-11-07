#ifndef KERNEL_VDSO_H
#define KERNEL_VDSO_H

#include <stdint.h>
#include <kernel/syscall.h>

void vdso_init(void);
void vdso_set_current_pid(uint32_t pid);
const struct ok_vdso_page *vdso_page(void);

#endif
