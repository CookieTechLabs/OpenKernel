#include <stdint.h>
#include <stddef.h>
#include <memory/page.h>
#include <kernel/multiboot.h>

#define PAGE_COUNT (0x100000000ull / MEMORY_PAGE_SIZE)
#define PAGE_BITMAP_WORDS (PAGE_COUNT / 32u)
#define PAGE_DIRECTORY_ENTRIES 1024u
#define PAGE_SIZE_4M 0x00400000u
#define MULTIBOOT_INFO_MEMORY_MAP (1u << 6)

static uint32_t page_bitmap[PAGE_BITMAP_WORDS];
static uint32_t total_pages;
static uint32_t free_pages;
static uint32_t next_page;
static uint32_t page_directory[PAGE_DIRECTORY_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t address;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

static void mark_page_used(uint32_t page) {
    uint32_t mask = 1u << (page & 31u);
    uint32_t *word = &page_bitmap[page >> 5];

    if ((*word & mask) == 0) {
        *word |= mask;
        if (free_pages != 0) {
            free_pages--;
        }
    }
}

static void mark_page_free(uint32_t page) {
    uint32_t mask = 1u << (page & 31u);
    uint32_t *word = &page_bitmap[page >> 5];

    if ((*word & mask) != 0) {
        *word &= ~mask;
        free_pages++;
    }
}

static int page_is_used(uint32_t page) {
    return (page_bitmap[page >> 5] & (1u << (page & 31u))) != 0;
}

static void reserve_range(uint64_t start, uint64_t length) {
    uint64_t end = start + length;
    uint32_t first;
    uint32_t last;

    if (end < start || start >= 0x100000000ull) {
        return;
    }
    if (end > 0x100000000ull) {
        end = 0x100000000ull;
    }

    first = (uint32_t)(start / MEMORY_PAGE_SIZE);
    last = (uint32_t)((end + MEMORY_PAGE_SIZE - 1) / MEMORY_PAGE_SIZE);
    if (last > PAGE_COUNT) {
        last = PAGE_COUNT;
    }
    for (uint32_t page = first; page < last; page++) {
        mark_page_used(page);
    }
}

static void enable_identity_paging(void) {
    for (uint32_t index = 0; index < PAGE_DIRECTORY_ENTRIES; index++) {
        page_directory[index] = (index * PAGE_SIZE_4M) | 0x083u;
    }

    uint32_t directory_address = (uint32_t)(uintptr_t)page_directory;
    uint32_t cr4;
    uint32_t cr0;

    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x10u;
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
    __asm__ volatile ("mov %0, %%cr3" : : "r"(directory_address) : "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

int memory_init(const struct multiboot_info *boot_info) {
    if (boot_info == 0) {
        return -1;
    }

    for (uint32_t index = 0; index < PAGE_BITMAP_WORDS; index++) {
        page_bitmap[index] = 0xFFFFFFFFu;
    }
    total_pages = 0;
    free_pages = 0;

    if ((boot_info->flags & MULTIBOOT_INFO_MEMORY_MAP) != 0 &&
        boot_info->mmap_addr != 0 && boot_info->mmap_length != 0) {
        uint32_t offset = 0;

        while (offset + sizeof(uint32_t) <= boot_info->mmap_length) {
            const struct multiboot_mmap_entry *entry =
                (const struct multiboot_mmap_entry *)(uintptr_t)
                    (boot_info->mmap_addr + offset);
            uint32_t entry_size = entry->size;

            if (entry_size < 20u ||
                entry_size > boot_info->mmap_length - offset - sizeof(uint32_t)) {
                return -1;
            }
            if (entry->type == 1u && entry->length != 0) {
                if (entry->address >= 0x100000000ull) {
                    offset += entry_size + sizeof(uint32_t);
                    continue;
                }
                uint64_t start = (entry->address + MEMORY_PAGE_SIZE - 1) &
                                 ~((uint64_t)MEMORY_PAGE_SIZE - 1);
                uint64_t end = entry->address + entry->length;
                if (end < entry->address) {
                    end = 0x100000000ull;
                }
                if (end > 0x100000000ull) {
                    end = 0x100000000ull;
                }
                while (start < end) {
                    uint32_t page = (uint32_t)(start / MEMORY_PAGE_SIZE);
                    if (page_is_used(page)) {
                        total_pages++;
                    }
                    mark_page_free(page);
                    start += MEMORY_PAGE_SIZE;
                }
            }
            offset += entry_size + sizeof(uint32_t);
        }
    } else if ((boot_info->flags & 1u) != 0) {
        uint64_t end = 0x100000ull + (uint64_t)boot_info->mem_upper * 1024u;
        if (end > 0x100000000ull) {
            end = 0x100000000ull;
        }
        for (uint64_t address = 0x100000ull; address + MEMORY_PAGE_SIZE <= end;
             address += MEMORY_PAGE_SIZE) {
            uint32_t page = (uint32_t)(address / MEMORY_PAGE_SIZE);
            if (page_is_used(page)) {
                total_pages++;
            }
            mark_page_free(page);
        }
    } else {
        return -1;
    }

    reserve_range(0, 0x100000u);
    reserve_range((uintptr_t)&__kernel_start,
                  (uintptr_t)&__kernel_end - (uintptr_t)&__kernel_start);
    reserve_range((uintptr_t)boot_info, sizeof(*boot_info));
    if ((boot_info->flags & MULTIBOOT_INFO_MEMORY_MAP) != 0) {
        reserve_range(boot_info->mmap_addr, boot_info->mmap_length);
    }
    if ((boot_info->flags & (1u << 3)) != 0 && boot_info->mods_count != 0 &&
        boot_info->mods_addr != 0) {
        const struct multiboot_module *modules =
            (const struct multiboot_module *)(uintptr_t)boot_info->mods_addr;
        reserve_range(boot_info->mods_addr,
                      boot_info->mods_count * sizeof(*modules));
        for (uint32_t index = 0; index < boot_info->mods_count; index++) {
            if (modules[index].mod_end >= modules[index].mod_start) {
                reserve_range(modules[index].mod_start,
                              modules[index].mod_end - modules[index].mod_start);
            }
        }
    }

    mark_page_used(0);
    next_page = 0x100000u / MEMORY_PAGE_SIZE;
    enable_identity_paging();
    return 0;
}

void *memory_alloc_page(void) {
    return memory_alloc_pages(1);
}

void *memory_alloc_pages(uint32_t count) {
    if (count == 0 || count > free_pages) {
        return 0;
    }

    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t first = pass == 0 ? next_page : 0;
        uint32_t limit = pass == 0 ? (uint32_t)PAGE_COUNT : next_page;
        for (uint32_t page = first; page < limit;) {
            uint32_t run = 0;
            while (run < count && page + run < limit &&
                   !page_is_used(page + run)) {
                run++;
            }
            if (run == count) {
                for (uint32_t index = 0; index < count; index++) {
                    mark_page_used(page + index);
                }
                next_page = page + count;
                if (next_page >= PAGE_COUNT) {
                    next_page = 0;
                }
                return (void *)(uintptr_t)(page * MEMORY_PAGE_SIZE);
            }
            page += run + 1;
        }
    }
    return 0;
}

int memory_free_page(void *address) {
    return memory_free_pages_at(address, 1);
}

int memory_free_pages_at(void *address, uint32_t count) {
    uintptr_t value = (uintptr_t)address;
    uint32_t first;

    if (count == 0 || value == 0 ||
        (value & (MEMORY_PAGE_SIZE - 1u)) != 0 ||
        count > PAGE_COUNT || value / MEMORY_PAGE_SIZE > PAGE_COUNT - count) {
        return -1;
    }
    first = (uint32_t)(value / MEMORY_PAGE_SIZE);
    for (uint32_t index = 0; index < count; index++) {
        if (!page_is_used(first + index)) {
            return -1;
        }
    }
    for (uint32_t index = 0; index < count; index++) {
        mark_page_free(first + index);
    }
    if (first < next_page) {
        next_page = first;
    }
    return 0;
}

uint32_t memory_total_pages(void) {
    return total_pages;
}

uint32_t memory_free_pages(void) {
    return free_pages;
}
