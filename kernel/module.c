#include <kernel/module.h>
#include <kernel.h>
#include <kernel/tar.h>
#include <kernel/time.h>
#include <memory/page.h>
#include "../fs/vfs.h"

#define MAX_KERNEL_MODULES 128u
#define MAX_MODULE_SECTIONS 128u
#define MAX_MODULE_SIZE (4u * 1024u * 1024u)

#define ELF_SHT_PROGBITS 1u
#define ELF_SHT_SYMTAB 2u
#define ELF_SHT_STRTAB 3u
#define ELF_SHT_NOBITS 8u
#define ELF_SHT_REL 9u
#define ELF_SHF_ALLOC 0x2u
#define ELF_SHN_UNDEF 0u
#define ELF_SHN_ABS 0xFFF1u
#define ELF_R_386_NONE 0u
#define ELF_R_386_32 1u
#define ELF_R_386_PC32 2u

struct elf32_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t program_offset;
    uint32_t section_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t section_entry_size;
    uint16_t section_count;
    uint16_t section_names_index;
} __attribute__((packed));

struct elf32_section {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t address;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t alignment;
    uint32_t entry_size;
} __attribute__((packed));

struct elf32_symbol {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t info;
    uint8_t other;
    uint16_t section_index;
} __attribute__((packed));

struct elf32_rel {
    uint32_t offset;
    uint32_t info;
} __attribute__((packed));

struct loaded_module {
    kernel_module_t *descriptor;
    void *allocation;
    uint32_t pages;
};

struct module_section {
    struct elf32_section header;
    uint32_t offset;
    uint8_t *address;
};

static struct loaded_module module_table[MAX_KERNEL_MODULES];
static uint32_t module_count;
static uint32_t initrd_base;
static int module_system_ready;

static int range_valid(uint32_t offset, uint32_t size, uint32_t file_size) {
    return offset <= file_size && size <= file_size - offset;
}

static int bounded_string(const char *table, uint32_t table_size,
                          uint32_t offset) {
    if (offset >= table_size) {
        return 0;
    }
    while (offset < table_size && table[offset] != '\0') {
        offset++;
    }
    return offset < table_size;
}

static uintptr_t resolve_kernel_symbol(const char *name) {
    static const struct {
        const char *name;
        uintptr_t address;
    } exports[] = {
        { "k_print", (uintptr_t)k_print },
        { "k_print_char", (uintptr_t)k_print_char },
        { "sleep_ms", (uintptr_t)sleep_ms }
    };

    if (name == 0) {
        return 0;
    }
    for (uint32_t index = 0; index < sizeof(exports) / sizeof(exports[0]); index++) {
        uint32_t character = 0;
        while (exports[index].name[character] == name[character] &&
               name[character] != '\0') {
            character++;
        }
        if (exports[index].name[character] == '\0' &&
            name[character] == '\0') {
            return exports[index].address;
        }
    }
    return 0;
}

static int module_name_matches(const char *left, const char *right) {
    if (left == 0 || right == 0) {
        return 0;
    }
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static int module_insert_owned(kernel_module_t *descriptor, void *allocation,
                               uint32_t pages) {
    if (descriptor == 0 || descriptor->name == 0 ||
        descriptor->name[0] == '\0' || descriptor->init == 0 ||
        module_count >= MAX_KERNEL_MODULES) {
        return -1;
    }
    for (uint32_t index = 0; index < module_count; index++) {
        if (module_name_matches(module_table[index].descriptor->name,
                                descriptor->name)) {
            return -1;
        }
    }
    if (descriptor->init() != 0) {
        return -1;
    }
    module_table[module_count].descriptor = descriptor;
    module_table[module_count].allocation = allocation;
    module_table[module_count].pages = pages;
    module_count++;
    k_print("[MODULE] Inserted ");
    k_print(descriptor->name);
    k_print("\n");
    return 0;
}

int module_insert(kernel_module_t *mod_addr) {
    return module_insert_owned(mod_addr, 0, 0);
}

int __internal_insert_mod(kernel_module_t *mod_addr) {
    return module_insert(mod_addr);
}

int module_remove(const char *name) {
    uint32_t index;

    if (name == 0 || name[0] == '\0') {
        return -1;
    }
    for (index = 0; index < module_count; index++) {
        if (module_name_matches(module_table[index].descriptor->name, name)) {
            struct loaded_module removed = module_table[index];
            if (removed.descriptor->exit != 0) {
                removed.descriptor->exit();
            }
            for (uint32_t move = index + 1; move < module_count; move++) {
                module_table[move - 1] = module_table[move];
            }
            module_count--;
            module_table[module_count].descriptor = 0;
            module_table[module_count].allocation = 0;
            module_table[module_count].pages = 0;
            if (removed.allocation != 0 &&
                memory_free_pages_at(removed.allocation, removed.pages) != 0) {
                return -1;
            }
            return 0;
        }
    }
    return -1;
}

static int load_elf_module(const void *image, uint32_t file_size) {
    const uint8_t *bytes = (const uint8_t *)image;
    const struct elf32_header *header;
    struct module_section sections[MAX_MODULE_SECTIONS];
    uint32_t section_names_size;
    const char *section_names;
    uint32_t total_size = 0;
    uint32_t pages;
    uint8_t *allocation;
    int descriptor_section = -1;

    if (image == 0 || file_size < sizeof(*header)) {
        return -1;
    }
    header = (const struct elf32_header *)image;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != 1 || header->ident[5] != 1 ||
        header->type != 1 || header->machine != 3 ||
        header->section_entry_size != sizeof(struct elf32_section) ||
        header->section_count == 0 ||
        header->section_count > MAX_MODULE_SECTIONS ||
        header->section_names_index >= header->section_count ||
        !range_valid(header->section_offset,
                     header->section_count * sizeof(struct elf32_section),
                     file_size)) {
        return -1;
    }

    const struct elf32_section *raw_sections =
        (const struct elf32_section *)(bytes + header->section_offset);
    const struct elf32_section *name_header =
        &raw_sections[header->section_names_index];
    if (name_header->type != ELF_SHT_STRTAB ||
        !range_valid(name_header->offset, name_header->size, file_size)) {
        return -1;
    }
    section_names_size = name_header->size;
    section_names = (const char *)(bytes + name_header->offset);

    for (uint32_t index = 0; index < header->section_count; index++) {
        sections[index].header = raw_sections[index];
        sections[index].offset = 0;
        sections[index].address = 0;
        if (!bounded_string(section_names, section_names_size,
                            sections[index].header.name)) {
            return -1;
        }
        if ((sections[index].header.flags & ELF_SHF_ALLOC) == 0) {
            continue;
        }
        if (sections[index].header.type != ELF_SHT_PROGBITS &&
            sections[index].header.type != ELF_SHT_NOBITS) {
            return -1;
        }
        uint32_t alignment = sections[index].header.alignment;
        if (alignment == 0) {
            alignment = 1;
        }
        if ((alignment & (alignment - 1u)) != 0 ||
            alignment > MAX_MODULE_SIZE ||
            total_size > MAX_MODULE_SIZE - alignment) {
            return -1;
        }
        total_size = (total_size + alignment - 1u) & ~(alignment - 1u);
        sections[index].offset = total_size;
        if (sections[index].header.size > MAX_MODULE_SIZE - total_size) {
            return -1;
        }
        total_size += sections[index].header.size;
        if (section_names[sections[index].header.name] == '.' &&
            module_name_matches(section_names + sections[index].header.name,
                                ".kernel_modules")) {
            descriptor_section = (int)index;
        }
    }
    if (descriptor_section < 0 ||
        sections[descriptor_section].header.size < sizeof(kernel_module_t) ||
        total_size == 0) {
        return -1;
    }
    pages = (total_size + MEMORY_PAGE_SIZE - 1u) / MEMORY_PAGE_SIZE;
    allocation = (uint8_t *)memory_alloc_pages(pages);
    if (allocation == 0) {
        return -1;
    }

    for (uint32_t index = 0; index < total_size; index++) {
        allocation[index] = 0;
    }
    for (uint32_t index = 0; index < header->section_count; index++) {
        if ((sections[index].header.flags & ELF_SHF_ALLOC) == 0) {
            continue;
        }
        sections[index].address = allocation + sections[index].offset;
        if (sections[index].header.type == ELF_SHT_PROGBITS) {
            if (!range_valid(sections[index].header.offset,
                             sections[index].header.size, file_size)) {
                memory_free_pages_at(allocation, pages);
                return -1;
            }
            for (uint32_t byte = 0; byte < sections[index].header.size; byte++) {
                sections[index].address[byte] =
                    bytes[sections[index].header.offset + byte];
            }
        }
    }

    for (uint32_t index = 0; index < header->section_count; index++) {
        const struct elf32_section *rel_section = &sections[index].header;
        if (rel_section->type != ELF_SHT_REL) {
            continue;
        }
        if (rel_section->info >= header->section_count ||
            rel_section->link >= header->section_count ||
            (rel_section->size % sizeof(struct elf32_rel)) != 0 ||
            !range_valid(rel_section->offset, rel_section->size, file_size)) {
            memory_free_pages_at(allocation, pages);
            return -1;
        }
        struct module_section *target = &sections[rel_section->info];
        const struct elf32_section *symtab = &sections[rel_section->link].header;
        if ((target->header.flags & ELF_SHF_ALLOC) == 0) {
            continue;
        }
        if (target->address == 0 ||
            target->header.size < sizeof(uint32_t) ||
            symtab->type != ELF_SHT_SYMTAB ||
            symtab->entry_size != sizeof(struct elf32_symbol) ||
            symtab->link >= header->section_count ||
            (symtab->size % sizeof(struct elf32_symbol)) != 0 ||
            !range_valid(symtab->offset, symtab->size, file_size)) {
            memory_free_pages_at(allocation, pages);
            return -1;
        }
        const struct elf32_section *strtab = &sections[symtab->link].header;
        if (strtab->type != ELF_SHT_STRTAB ||
            !range_valid(strtab->offset, strtab->size, file_size)) {
            memory_free_pages_at(allocation, pages);
            return -1;
        }
        const char *symbols_names = (const char *)(bytes + strtab->offset);
        const struct elf32_symbol *symbols =
            (const struct elf32_symbol *)(bytes + symtab->offset);
        const struct elf32_rel *relocations =
            (const struct elf32_rel *)(bytes + rel_section->offset);
        uint32_t symbol_count = symtab->size / sizeof(*symbols);
        uint32_t relocation_count = rel_section->size / sizeof(*relocations);

        for (uint32_t relocation_index = 0;
             relocation_index < relocation_count; relocation_index++) {
            const struct elf32_rel *rel = &relocations[relocation_index];
            uint32_t symbol_index = rel->info >> 8;
            uint32_t relocation_type = rel->info & 0xFFu;
            if (relocation_type == ELF_R_386_NONE) {
                continue;
            }
            if ((relocation_type != ELF_R_386_32 &&
                 relocation_type != ELF_R_386_PC32) ||
                symbol_index >= symbol_count ||
                rel->offset > target->header.size - sizeof(uint32_t)) {
                memory_free_pages_at(allocation, pages);
                return -1;
            }

            const struct elf32_symbol *symbol = &symbols[symbol_index];
            uintptr_t symbol_address = 0;
            if (symbol->section_index == ELF_SHN_ABS) {
                symbol_address = symbol->value;
            } else if (symbol->section_index == ELF_SHN_UNDEF) {
                if (!bounded_string(symbols_names, strtab->size, symbol->name)) {
                    memory_free_pages_at(allocation, pages);
                    return -1;
                }
                symbol_address = resolve_kernel_symbol(symbols_names + symbol->name);
                if (symbol_address == 0) {
                    memory_free_pages_at(allocation, pages);
                    return -1;
                }
            } else if (symbol->section_index < header->section_count &&
                       sections[symbol->section_index].address != 0 &&
                       symbol->value <=
                           sections[symbol->section_index].header.size) {
                symbol_address =
                    (uintptr_t)sections[symbol->section_index].address +
                    symbol->value;
            } else {
                memory_free_pages_at(allocation, pages);
                return -1;
            }

            uint32_t *place = (uint32_t *)(target->address + rel->offset);
            uintptr_t addend = *place;
            uintptr_t value = symbol_address + addend;
            if (relocation_type == ELF_R_386_PC32) {
                value -= (uintptr_t)place;
            }
            *place = (uint32_t)value;
        }
    }

    kernel_module_t *descriptor = (kernel_module_t *)
        sections[descriptor_section].address;
    if (module_insert_owned(descriptor, allocation, pages) != 0) {
        memory_free_pages_at(allocation, pages);
        return -1;
    }
    return 0;
}

int module_load_from_initrd(const char *path) {
    uint32_t size = 0;
    const char *image;

    if (path == 0 || path[0] == '\0' || initrd_base == 0) {
        return -1;
    }
    image = tar_get_file(initrd_base, path, &size);
    if (image == 0) {
        return -1;
    }
    return load_elf_module(image, size);
}

static int has_module_extension(const char *name) {
    uint32_t length = 0;
    while (name[length] != '\0') {
        length++;
    }
    return length > 3 && name[length - 3] == '.' &&
           name[length - 2] == 'k' && name[length - 1] == 'o';
}

static void load_module_tree(struct vfs_node *directory, char *path,
                             uint32_t path_length, uint32_t depth) {
    if (directory == 0 || depth > 8) {
        return;
    }
    for (struct vfs_node *child = directory->children; child != 0;
         child = child->next) {
        uint32_t name_length = 0;
        while (child->name[name_length] != '\0' &&
               name_length < VFS_NAME_SIZE) {
            name_length++;
        }
        if (name_length == 0 ||
            path_length + name_length + 2 >= VFS_MAX_PATH) {
            continue;
        }
        uint32_t child_path_length = path_length;
        path[child_path_length++] = '/';
        for (uint32_t index = 0; index < name_length; index++) {
            path[child_path_length++] = child->name[index];
        }
        path[child_path_length] = '\0';
        if (child->flags & VFS_DIRECTORY) {
            load_module_tree(child, path, child_path_length, depth + 1);
        } else if (has_module_extension(child->name)) {
            if (module_load_from_initrd(path) != 0) {
                k_print("[MODULE] Failed to insert ");
                k_print(path);
                k_print("\n");
            }
        }
        path[path_length] = '\0';
    }
}

int module_load_all_from_initrd(void) {
    struct vfs_node *directory = vfs_lookup(vfs_get_root(), "/lib/modules");
    char path[VFS_MAX_PATH] = "/lib/modules";

    if (directory == 0 || !(directory->flags & VFS_DIRECTORY)) {
        return 0;
    }
    load_module_tree(directory, path, 12, 0);
    return 0;
}

int module_system_init(void) {
    if (module_system_ready) {
        return 0;
    }
    module_system_ready = 1;
    module_count = 0;
    return 0;
}

void module_set_initrd(uint32_t address) {
    initrd_base = address;
}
