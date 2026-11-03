#include "e1000.h"
#include <kernel/ports.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC
#define E1000_VENDOR_ID    0x8086

#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_EERD   0x0014
#define REG_IMC    0x00D8
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400
#define REG_TIPG   0x0410
#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818
#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818
#define REG_RAL    0x5400
#define REG_RAH    0x5404

#define E1000_RING_COUNT 8u
#define E1000_BUFFER_SIZE 2048u
#define E1000_RX_DONE 0x01u
#define E1000_TX_DONE 0x01u
#define E1000_TX_CMD_EOP  0x01u
#define E1000_TX_CMD_IFCS 0x02u
#define E1000_TX_CMD_RS   0x08u

struct e1000_rx_descriptor {
	uint64_t address;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
} __attribute__((packed));

struct e1000_tx_descriptor {
	uint64_t address;
	uint16_t length;
	uint8_t checksum_offset;
	uint8_t command;
	uint8_t status;
	uint8_t checksum_start;
	uint16_t special;
} __attribute__((packed));

static volatile uint8_t *registers;
static struct e1000_rx_descriptor rx_descriptors[E1000_RING_COUNT]
	__attribute__((aligned(16)));
static struct e1000_tx_descriptor tx_descriptors[E1000_RING_COUNT]
	__attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_RING_COUNT][E1000_BUFFER_SIZE]
	__attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_RING_COUNT][E1000_BUFFER_SIZE]
	__attribute__((aligned(16)));
static uint32_t rx_index;
static uint32_t tx_index;
static int initialized;

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function,
						   uint8_t offset) {
	uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
					   ((uint32_t)slot << 11) |
					   ((uint32_t)function << 8) | (offset & 0xFCu);
	uint32_t value;

	__asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
	__asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(PCI_CONFIG_DATA));
	return value;
}

static void pci_write16(uint8_t bus, uint8_t slot, uint8_t function,
						uint8_t offset, uint16_t value) {
	uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
					   ((uint32_t)slot << 11) |
					   ((uint32_t)function << 8) | (offset & 0xFCu);
	uint16_t old_value;
	uint16_t shift = (uint16_t)((offset & 2u) * 8u);
	uint32_t data;

	__asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
	__asm__ volatile ("inl %1, %0" : "=a"(data) : "Nd"(PCI_CONFIG_DATA));
	old_value = (uint16_t)(data >> shift);
	data = (data & ~((uint32_t)0xFFFFu << shift)) |
		   ((uint32_t)(old_value | value) << shift);
	__asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
	__asm__ volatile ("outl %0, %1" : : "a"(data), "Nd"(PCI_CONFIG_DATA));
}

static int find_device(uint8_t *found_bus, uint8_t *found_slot,
					   uint8_t *found_function, uint32_t *bar) {
	for (uint8_t slot = 0; slot < 32; slot++) {
		for (uint8_t function = 0; function < 8; function++) {
			uint32_t id = pci_read32(0, slot, function, 0);
			uint16_t vendor = (uint16_t)id;
			uint16_t device = (uint16_t)(id >> 16);

			if (vendor == 0xFFFFu) {
				if (function == 0) {
					break;
				}
				continue;
			}
			if (vendor == E1000_VENDOR_ID &&
				(device == 0x100Eu || device == 0x100Fu ||
				 device == 0x10D3u)) {
				*found_bus = 0;
				*found_slot = slot;
				*found_function = function;
				*bar = pci_read32(0, slot, function, 0x10);
				return 0;
			}
			if (function == 0 &&
				((pci_read32(0, slot, 0, 0x0C) >> 16) & 0x80u) == 0) {
				break;
			}
		}
	}
	return -1;
}

static uint32_t reg_read(uint32_t offset) {
	uint32_t value = *(volatile uint32_t *)(registers + offset);
	__asm__ volatile ("" : : : "memory");
	return value;
}

static void reg_write(uint32_t offset, uint32_t value) {
	__asm__ volatile ("" : : : "memory");
	*(volatile uint32_t *)(registers + offset) = value;
}

static int read_mac_eeprom(uint8_t mac_address[6]) {
	for (uint32_t word = 0; word < 3; word++) {
		uint32_t timeout = 100000;
		reg_write(REG_EERD, (word << 8) | 1u);
		while (timeout-- != 0) {
			uint32_t result = reg_read(REG_EERD);
			if (result & (1u << 4)) {
				uint16_t data = (uint16_t)(result >> 16);
				mac_address[word * 2] = (uint8_t)data;
				mac_address[word * 2 + 1] = (uint8_t)(data >> 8);
				break;
			}
		}
		if (timeout == 0) {
			return -1;
		}
	}
	return 0;
}

int e1000_init(uint8_t mac_address[6]) {
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
	uint32_t bar;
	uint32_t mac_low;
	uint32_t mac_high;

	if (mac_address == 0 || initialized) {
		return initialized ? 0 : -1;
	}
	if (find_device(&bus, &slot, &function, &bar) != 0 ||
		(bar & 1u) != 0 || (bar & 6u) == 4u) {
		return -1;
	}

	pci_write16(bus, slot, function, 0x04, 0x0006u);
	registers = (volatile uint8_t *)(uintptr_t)(bar & ~0xFu);
	if (registers == 0) {
		return -1;
	}
	reg_write(REG_IMC, 0xFFFFFFFFu);
	reg_write(REG_CTRL, reg_read(REG_CTRL) | (1u << 26));
	for (volatile uint32_t delay = 0; delay < 100000; delay++) {
		__asm__ volatile ("pause");
	}

	mac_low = reg_read(REG_RAL);
	mac_high = reg_read(REG_RAH);
	if ((mac_high & (1u << 31)) == 0 || (mac_low == 0 && mac_high == 0)) {
		if (read_mac_eeprom(mac_address) != 0) {
			return -1;
		}
	} else {
		mac_address[0] = (uint8_t)mac_low;
		mac_address[1] = (uint8_t)(mac_low >> 8);
		mac_address[2] = (uint8_t)(mac_low >> 16);
		mac_address[3] = (uint8_t)(mac_low >> 24);
		mac_address[4] = (uint8_t)mac_high;
		mac_address[5] = (uint8_t)(mac_high >> 8);
	}

	for (uint32_t index = 0; index < E1000_RING_COUNT; index++) {
		rx_descriptors[index].address =
			(uint32_t)(uintptr_t)&rx_buffers[index][0];
		rx_descriptors[index].status = 0;
		tx_descriptors[index].address =
			(uint32_t)(uintptr_t)&tx_buffers[index][0];
		tx_descriptors[index].status = E1000_TX_DONE;
	}
	rx_index = 0;
	tx_index = 0;

	reg_write(REG_RCTL, 0);
	reg_write(REG_TCTL, 0);
	reg_write(REG_RDBAL, (uint32_t)(uintptr_t)rx_descriptors);
	reg_write(REG_RDBAH, 0);
	reg_write(REG_RDLEN, sizeof(rx_descriptors));
	reg_write(REG_RDH, 0);
	reg_write(REG_RDT, E1000_RING_COUNT - 1);
	reg_write(REG_TDBAL, (uint32_t)(uintptr_t)tx_descriptors);
	reg_write(REG_TDBAH, 0);
	reg_write(REG_TDLEN, sizeof(tx_descriptors));
	reg_write(REG_TDH, 0);
	reg_write(REG_TDT, 0);
	reg_write(REG_TIPG, 0x0060200Au);
	reg_write(REG_TCTL, (1u << 1) | (1u << 3) | (0x0Fu << 4) |
			  (0x40u << 12));
	reg_write(REG_RCTL, (1u << 1) | (1u << 15) | (1u << 26));
	initialized = 1;
	return 0;
}

int e1000_send(const void *frame, uint16_t length) {
	struct e1000_tx_descriptor *descriptor;
	uint32_t timeout = 1000000;

	if (!initialized || frame == 0 || length < 14 ||
		length > E1000_FRAME_MAX) {
		return -1;
	}
	descriptor = &tx_descriptors[tx_index];
	while ((descriptor->status & E1000_TX_DONE) == 0 && timeout-- != 0) {
		__asm__ volatile ("pause");
	}
	if ((descriptor->status & E1000_TX_DONE) == 0) {
		return -1;
	}

	for (uint32_t index = 0; index < length; index++) {
		tx_buffers[tx_index][index] = ((const uint8_t *)frame)[index];
	}
	descriptor->length = length;
	descriptor->checksum_offset = 0;
	descriptor->checksum_start = 0;
	descriptor->special = 0;
	descriptor->command = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS |
						  E1000_TX_CMD_RS;
	descriptor->status = 0;
	__asm__ volatile ("" : : : "memory");
	tx_index = (tx_index + 1) % E1000_RING_COUNT;
	reg_write(REG_TDT, tx_index);
	return (int)length;
}

int e1000_receive(void *frame, uint16_t capacity) {
	struct e1000_rx_descriptor *descriptor;
	uint16_t length;

	if (!initialized || frame == 0) {
		return -1;
	}
	descriptor = &rx_descriptors[rx_index];
	__asm__ volatile ("" : : : "memory");
	if ((descriptor->status & E1000_RX_DONE) == 0) {
		return 0;
	}
	length = descriptor->length;
	if (length > capacity || length > E1000_FRAME_MAX ||
		descriptor->errors != 0) {
		length = 0;
	} else {
		for (uint32_t index = 0; index < length; index++) {
			((uint8_t *)frame)[index] = rx_buffers[rx_index][index];
		}
	}
	descriptor->status = 0;
	descriptor->errors = 0;
	__asm__ volatile ("" : : : "memory");
	reg_write(REG_RDT, rx_index);
	rx_index = (rx_index + 1) % E1000_RING_COUNT;
	return (int)length;
}
