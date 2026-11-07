#include <kernel/ata.h>
#include <kernel/ports.h>
#include <stdint.h>

#define ATA_DATA          0x1F0
#define ATA_ERROR         0x1F1
#define ATA_SECTOR_COUNT  0x1F2
#define ATA_LBA_LOW       0x1F3
#define ATA_LBA_MID       0x1F4
#define ATA_LBA_HIGH      0x1F5
#define ATA_DRIVE         0x1F6
#define ATA_STATUS        0x1F7
#define ATA_COMMAND       0x1F7

#define ATA_STATUS_ERROR  0x01
#define ATA_STATUS_DRQ    0x08
#define ATA_STATUS_DF     0x20
#define ATA_STATUS_BUSY   0x80

#define ATA_CMD_READ      0x20
#define ATA_CMD_WRITE     0x30
#define ATA_CMD_FLUSH     0xE7

#define ATA_LBA28_LIMIT   0x10000000
#define ATA_SECTOR_WORDS  256

#define ATA_TIMEOUT       1000000

static void ata_insw(uint16_t port, uint16_t *buffer, uint32_t count)
{
    __asm__ volatile (
        "cld; rep insw"
        : "+D"(buffer), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static void ata_outsw(uint16_t port, const uint16_t *buffer, uint32_t count)
{
    __asm__ volatile (
        "cld; rep outsw"
        : "+S"(buffer), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

/*
 * ATA requires approximately 400 ns after device selection.
 * Four alternate status reads provide the required delay.
 */
static void ata_delay(void)
{
    port_byte_in(ATA_STATUS);
    port_byte_in(ATA_STATUS);
    port_byte_in(ATA_STATUS);
    port_byte_in(ATA_STATUS);
}

/*
 * Wait until the device is not busy and the requested status
 * bits are present.
 *
 * Returns:
 *   0  Success
 *  -1  Timeout, missing device, or device error
 */
static int ata_wait(uint8_t required)
{
    uint32_t timeout = ATA_TIMEOUT;

    while (timeout-- != 0) {
        uint8_t status = port_byte_in(ATA_STATUS);

        if (status == 0x00 || status == 0xFF) {
            return -1;
        }

        if (status & ATA_STATUS_BUSY) {
            continue;
        }

        if (status & (ATA_STATUS_ERROR | ATA_STATUS_DF)) {
            (void)port_byte_in(ATA_ERROR);
            return -1;
        }

        if ((status & required) == required) {
            return 0;
        }
    }

    return -1;
}

/*
 * Select the primary master ATA device using LBA28 addressing.
 */
static int ata_select(uint32_t lba)
{
    port_byte_out(
        ATA_DRIVE,
        (uint8_t)(0xE0 | ((lba >> 24) & 0x0F))
    );

    ata_delay();

    return ata_wait(0);
}

/*
 * Set the LBA28 sector address and sector count.
 */
static void ata_set_lba(uint32_t lba)
{
    port_byte_out(ATA_SECTOR_COUNT, 1);
    port_byte_out(ATA_LBA_LOW, (uint8_t)lba);
    port_byte_out(ATA_LBA_MID, (uint8_t)(lba >> 8));
    port_byte_out(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
}

/*
 * Read one 512-byte sector.
 */
int ata_read_sector(uint32_t lba, void *buffer)
{
    if (buffer == 0 || lba >= ATA_LBA28_LIMIT) {
        return -1;
    }

    if (ata_select(lba) != 0) {
        return -1;
    }

    ata_set_lba(lba);

    /* Wait until the device accepts another command. */
    if (ata_wait(0) != 0) {
        return -1;
    }

    port_byte_out(ATA_COMMAND, ATA_CMD_READ);

    /* Wait for the data request. */
    if (ata_wait(ATA_STATUS_DRQ) != 0) {
        return -1;
    }

    ata_insw(
        ATA_DATA,
        (uint16_t *)buffer,
        ATA_SECTOR_WORDS
    );

    /* Ensure the device has completed the transfer. */
    if (ata_wait(0) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Write one 512-byte sector.
 */
int ata_write_sector(uint32_t lba, const void *buffer)
{
    if (buffer == 0 || lba >= ATA_LBA28_LIMIT) {
        return -1;
    }

    if (ata_select(lba) != 0) {
        return -1;
    }

    ata_set_lba(lba);

    if (ata_wait(0) != 0) {
        return -1;
    }

    port_byte_out(ATA_COMMAND, ATA_CMD_WRITE);

    /* Wait until the device requests sector data. */
    if (ata_wait(ATA_STATUS_DRQ) != 0) {
        return -1;
    }

    ata_outsw(
        ATA_DATA,
        (const uint16_t *)buffer,
        ATA_SECTOR_WORDS
    );

    /* Wait for the device to finish writing. */
    if (ata_wait(0) != 0) {
        return -1;
    }

    /* Flush the device write cache. */
    port_byte_out(ATA_COMMAND, ATA_CMD_FLUSH);

    if (ata_wait(0) != 0) {
        return -1;
    }

    return 0;
}