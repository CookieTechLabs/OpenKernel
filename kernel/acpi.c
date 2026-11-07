#include <stdint.h>
#include <stddef.h>

#include <kernel/acpi.h>

#define ACPI_RSDP_SIGNATURE "RSD PTR "
#define ACPI_FADT_SIGNATURE "FACP"
#define ACPI_RSDT_SIGNATURE "RSDT"

#define ACPI_SYSTEM_MEMORY 0
#define ACPI_SYSTEM_IO     1

#define ACPI_RESET_PORT 0x64
#define ACPI_RESET_COMMAND 0xFE

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct acpi_rsdp_extended {
    struct acpi_rsdp first;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_gas {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

struct acpi_fadt {
    struct acpi_sdt_header header;
    uint8_t reserved[80];
    struct acpi_gas reset_register;
    uint8_t reset_value;
} __attribute__((packed));

static struct acpi_gas reset_register;
static uint8_t reset_value;
static int reset_available;

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static int checksum_valid(const void *data, uint32_t length)
{
    const uint8_t *bytes = data;
    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; i++)
        sum += bytes[i];

    return sum == 0;
}

static struct acpi_rsdp *find_rsdp_range(uint32_t start, uint32_t end)
{
    for (uint32_t address = start; address + 20 <= end; address += 16) {
        struct acpi_rsdp *rsdp =
            (struct acpi_rsdp *)(uintptr_t)address;

        if (*(uint64_t *)rsdp != *(const uint64_t *)ACPI_RSDP_SIGNATURE)
            continue;

        if (!checksum_valid(rsdp, 20))
            continue;

        if (rsdp->revision >= 2) {
            struct acpi_rsdp_extended *extended =
                (struct acpi_rsdp_extended *)rsdp;

            if (extended->length < sizeof(*extended) ||
                extended->length > 4096)
                continue;

            if (!checksum_valid(extended, extended->length))
                continue;
        }

        return rsdp;
    }

    return 0;
}

static struct acpi_rsdp *find_rsdp(void)
{
    uint16_t ebda_segment =
        *(volatile uint16_t *)(uintptr_t)0x40E;

    uint32_t ebda_address = (uint32_t)ebda_segment << 4;

    if (ebda_address >= 0x400 && ebda_address < 0xA0000) {
        struct acpi_rsdp *rsdp =
            find_rsdp_range(ebda_address, ebda_address + 1024);

        if (rsdp)
            return rsdp;
    }

    return find_rsdp_range(0xE0000, 0x100000);
}

static struct acpi_sdt_header *find_fadt(uint32_t rsdt_address)
{
    struct acpi_sdt_header *rsdt =
        (struct acpi_sdt_header *)(uintptr_t)rsdt_address;

    if (!rsdt || rsdt->length < sizeof(*rsdt))
        return 0;

    if (*(uint32_t *)rsdt->signature != *(const uint32_t *)ACPI_RSDT_SIGNATURE)
        return 0;

    if (!checksum_valid(rsdt, rsdt->length))
        return 0;

    uint32_t entries =
        (rsdt->length - sizeof(*rsdt)) / sizeof(uint32_t);

    uint32_t *table_addresses =
        (uint32_t *)((uintptr_t)rsdt + sizeof(*rsdt));

    for (uint32_t i = 0; i < entries; i++) {
        struct acpi_sdt_header *table =
            (struct acpi_sdt_header *)(uintptr_t)table_addresses[i];

        if (!table || table->length < sizeof(*table))
            continue;

        if (*(uint32_t *)table->signature !=
            *(const uint32_t *)ACPI_FADT_SIGNATURE)
            continue;

        if (!checksum_valid(table, table->length))
            continue;

        return table;
    }

    return 0;
}

int acpi_init(void)
{
    reset_available = 0;

    struct acpi_rsdp *rsdp = find_rsdp();

    if (!rsdp || !rsdp->rsdt_address)
        return 0;

    struct acpi_sdt_header *header =
        find_fadt(rsdp->rsdt_address);

    if (!header)
        return 0;

    if (header->length <
        offsetof(struct acpi_fadt, reset_value) + sizeof(uint8_t))
        return 0;

    struct acpi_fadt *fadt = (struct acpi_fadt *)header;

    if (fadt->reset_register.address == 0)
        return 0;

    if (fadt->reset_register.bit_offset != 0)
        return 0;

    if (fadt->reset_register.bit_width < 8 ||
        fadt->reset_register.bit_width > 32)
        return 0;

    if (fadt->reset_register.address_space != ACPI_SYSTEM_IO &&
        fadt->reset_register.address_space != ACPI_SYSTEM_MEMORY)
        return 0;

    if (fadt->reset_register.address > 0xFFFFFFFFULL)
        return 0;

    if (fadt->reset_register.address_space == ACPI_SYSTEM_IO &&
        fadt->reset_register.address > 0xFFFF)
        return 0;

    reset_register = fadt->reset_register;
    reset_value = fadt->reset_value;
    reset_available = 1;

    return 1;
}

int acpi_reboot_supported(void)
{
    return reset_available;
}

static void acpi_reset(void)
{
    uint32_t address = (uint32_t)reset_register.address;
    uint32_t access_size = reset_register.access_size;

    if (access_size == 0) {
        if (reset_register.bit_width <= 8)
            access_size = 1;
        else if (reset_register.bit_width <= 16)
            access_size = 2;
        else
            access_size = 3;
    }

    if (reset_register.address_space == ACPI_SYSTEM_IO) {
        switch (access_size) {
        case 1:
            outb((uint16_t)address, reset_value);
            break;

        case 2:
            __asm__ volatile (
                "outw %0, %1"
                :
                : "a"((uint16_t)reset_value),
                  "Nd"((uint16_t)address)
            );
            break;

        case 3:
            __asm__ volatile (
                "outl %0, %1"
                :
                : "a"((uint32_t)reset_value),
                  "Nd"((uint16_t)address)
            );
            break;

        default:
            return;
        }
    } else {
        volatile uint8_t *memory =
            (volatile uint8_t *)(uintptr_t)address;

        switch (access_size) {
        case 1:
            *memory = reset_value;
            break;

        case 2:
            *(volatile uint16_t *)memory = reset_value;
            break;

        case 3:
            *(volatile uint32_t *)memory = reset_value;
            break;

        default:
            return;
        }
    }
}

static void keyboard_controller_reset(void)
{
    uint32_t timeout = 1000000;

    while ((inb(0x64) & 0x02) && timeout--)
        __asm__ volatile ("pause");

    if (timeout)
        outb(ACPI_RESET_PORT, ACPI_RESET_COMMAND);
}

void acpi_reboot(void)
{
    __asm__ volatile ("cli");

    if (reset_available)
        acpi_reset();

    keyboard_controller_reset();

    for (;;)
        __asm__ volatile ("hlt");
}
