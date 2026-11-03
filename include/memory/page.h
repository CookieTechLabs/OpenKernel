#ifndef MEMORY_PAGE_H
#define MEMORY_PAGE_H

#include <stdint.h>
#include <kernel/multiboot.h>

#define MEMORY_PAGE_SIZE 4096u

int memory_init(const struct multiboot_info *boot_info);
void *memory_alloc_page(void);
int memory_free_page(void *page);
void *memory_alloc_pages(uint32_t count);
int memory_free_pages_at(void *first_page, uint32_t count);
uint32_t memory_total_pages(void);
uint32_t memory_free_pages(void);

#endif
