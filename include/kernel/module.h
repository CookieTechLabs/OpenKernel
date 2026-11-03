#ifndef KERNEL_MODULE_H
#define KERNEL_MODULE_H

#include <stdint.h>
#include <stddef.h>

#define __init __attribute__((__section__(".init.text")))
#define __exit __attribute__((__section__(".exit.text")))

typedef struct kernel_module {
    const char *name;
    int (*init)(void);
    void (*exit)(void);
    uint32_t flags;
} kernel_module_t;

int __internal_insert_mod(kernel_module_t *mod_addr);
int module_system_init(void);
int module_insert(kernel_module_t *mod_addr);
int module_remove(const char *name);
int module_load_from_initrd(const char *path);
int module_load_all_from_initrd(void);
void module_set_initrd(uint32_t address);

#define KERNEL_MODULE(mod_name, init_fn, exit_fn) \
    static kernel_module_t __mod_##mod_name \
        __attribute__((section(".kernel_modules"), used)) = { \
        .name = #mod_name, \
        .init = init_fn, \
        .exit = exit_fn, \
        .flags = 0 \
    };

#endif