#include <driver/net.h>
#include "e1000.h"

#define ETHERNET_HEADER_SIZE 14u
#define IPV4_HEADER_SIZE 20u
#define UDP_HEADER_SIZE 8u
#define ARP_CACHE_SIZE 8u
#define UDP_QUEUE_SIZE 8u
#define RAW_QUEUE_SIZE 8u
#define IPV4_ADDRESS 0x0A00020Fu
#define IPV4_NETMASK 0xFFFFFF00u
#define IPV4_GATEWAY 0x0A000202u

#define ETHERTYPE_IPV4 0x0800u
#define ETHERTYPE_ARP  0x0806u
#define IP_PROTOCOL_ICMP 1u
#define IP_PROTOCOL_UDP 17u

struct arp_entry {
	uint32_t address;
	uint8_t mac[6];
	uint8_t valid;
};

struct udp_entry {
	struct net_udp_packet packet;
	uint8_t valid;
};

struct raw_entry {
	uint16_t length;
	uint8_t data[NET_FRAME_MAX];
};

static uint8_t local_mac[6];
static struct arp_entry arp_cache[ARP_CACHE_SIZE];
static struct udp_entry udp_queue[UDP_QUEUE_SIZE];
static struct raw_entry raw_queue[RAW_QUEUE_SIZE];
static uint32_t arp_next;
static uint32_t udp_next;
static uint32_t raw_head;
static uint32_t raw_count;
static int device_ready;

static uint16_t read_be16(const uint8_t *data) {
	return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_be32(const uint8_t *data) {
	return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		   ((uint32_t)data[2] << 8) | data[3];
}

static void write_be16(uint8_t *data, uint16_t value) {
	data[0] = (uint8_t)(value >> 8);
	data[1] = (uint8_t)value;
}

static void write_be32(uint8_t *data, uint32_t value) {
	data[0] = (uint8_t)(value >> 24);
	data[1] = (uint8_t)(value >> 16);
	data[2] = (uint8_t)(value >> 8);
	data[3] = (uint8_t)value;
}

static uint16_t checksum(const uint8_t *data, uint32_t length,
						 uint32_t initial) {
	uint32_t sum = initial;

	while (length >= 2) {
		sum += read_be16(data);
		data += 2;
		length -= 2;
	}
	if (length != 0) {
		sum += (uint16_t)((uint16_t)data[0] << 8);
	}
	while (sum >> 16) {
		sum = (sum & 0xFFFFu) + (sum >> 16);
	}
	return (uint16_t)~sum;
}

static int mac_equal(const uint8_t left[6], const uint8_t right[6]) {
	for (uint32_t index = 0; index < 6; index++) {
		if (left[index] != right[index]) {
			return 0;
		}
	}
	return 1;
}

static void cache_arp(uint32_t address, const uint8_t mac[6]) {
	for (uint32_t index = 0; index < ARP_CACHE_SIZE; index++) {
		if (arp_cache[index].valid && arp_cache[index].address == address) {
			for (uint32_t byte = 0; byte < 6; byte++) {
				arp_cache[index].mac[byte] = mac[byte];
			}
			return;
		}
	}
	struct arp_entry *entry = &arp_cache[arp_next];
	entry->address = address;
	for (uint32_t index = 0; index < 6; index++) {
		entry->mac[index] = mac[index];
	}
	entry->valid = 1;
	arp_next = (arp_next + 1) % ARP_CACHE_SIZE;
}

static int lookup_arp(uint32_t address, uint8_t mac[6]) {
	for (uint32_t index = 0; index < ARP_CACHE_SIZE; index++) {
		if (arp_cache[index].valid && arp_cache[index].address == address) {
			for (uint32_t byte = 0; byte < 6; byte++) {
				mac[byte] = arp_cache[index].mac[byte];
			}
			return 0;
		}
	}
	return -1;
}

static int transmit_arp_request(uint32_t address) {
	static const uint8_t broadcast[6] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint8_t frame[42] = {0};

	for (uint32_t index = 0; index < 6; index++) {
		frame[index] = broadcast[index];
		frame[6 + index] = local_mac[index];
	}
	write_be16(frame + 12, ETHERTYPE_ARP);
	write_be16(frame + 14, 1);
	write_be16(frame + 16, ETHERTYPE_IPV4);
	frame[18] = 6;
	frame[19] = 4;
	write_be16(frame + 20, 1);
	for (uint32_t index = 0; index < 6; index++) {
		frame[22 + index] = local_mac[index];
	}
	write_be32(frame + 28, IPV4_ADDRESS);
	write_be32(frame + 38, address);
	return e1000_send(frame, sizeof(frame)) < 0 ? -1 : 0;
}

static int resolve_arp(uint32_t address, uint8_t mac[6]) {
	if (lookup_arp(address, mac) == 0) {
		return 0;
	}
	if (transmit_arp_request(address) != 0) {
		return -1;
	}
	for (uint32_t attempt = 0; attempt < 100000; attempt++) {
		net_poll();
		if (lookup_arp(address, mac) == 0) {
			return 0;
		}
		__asm__ volatile ("pause");
	}
	return -1;
}

static void send_arp_reply(const uint8_t *request, const uint8_t destination[6],
						   uint32_t destination_ip) {
	uint8_t frame[42] = {0};

	for (uint32_t index = 0; index < 6; index++) {
		frame[index] = destination[index];
		frame[6 + index] = local_mac[index];
		frame[22 + index] = local_mac[index];
		frame[32 + index] = destination[index];
	}
	write_be16(frame + 12, ETHERTYPE_ARP);
	write_be16(frame + 14, 1);
	write_be16(frame + 16, ETHERTYPE_IPV4);
	frame[18] = 6;
	frame[19] = 4;
	write_be16(frame + 20, 2);
	write_be32(frame + 28, IPV4_ADDRESS);
	write_be32(frame + 38, destination_ip);
	(void)request;
	(void)e1000_send(frame, sizeof(frame));
}

static void handle_arp(const uint8_t *frame, uint16_t length) {
	const uint8_t *arp = frame + ETHERNET_HEADER_SIZE;
	uint32_t sender_ip;
	uint32_t target_ip;
	uint16_t operation;

	if (length < ETHERNET_HEADER_SIZE + 28 || read_be16(arp) != 1 ||
		read_be16(arp + 2) != ETHERTYPE_IPV4 || arp[4] != 6 ||
		arp[5] != 4) {
		return;
	}
	operation = read_be16(arp + 6);
	sender_ip = read_be32(arp + 14);
	target_ip = read_be32(arp + 24);
	if (sender_ip != 0) {
		cache_arp(sender_ip, arp + 8);
	}
	if (operation == 1 && target_ip == IPV4_ADDRESS) {
		send_arp_reply(arp, arp + 8, sender_ip);
	}
}

static void send_ipv4_packet(const uint8_t destination_mac[6],
							 uint32_t destination_ip, uint8_t protocol,
							 const uint8_t *payload, uint16_t payload_length) {
	uint8_t frame[NET_FRAME_MAX] = {0};
	uint8_t *ip = frame + ETHERNET_HEADER_SIZE;
	uint16_t ip_length = IPV4_HEADER_SIZE + payload_length;

	if (ip_length > 1500) {
		return;
	}
	for (uint32_t index = 0; index < 6; index++) {
		frame[index] = destination_mac[index];
		frame[6 + index] = local_mac[index];
	}
	write_be16(frame + 12, ETHERTYPE_IPV4);
	ip[0] = 0x45;
	ip[1] = 0;
	write_be16(ip + 2, ip_length);
	write_be16(ip + 4, 0);
	write_be16(ip + 6, 0x4000);
	ip[8] = 64;
	ip[9] = protocol;
	write_be16(ip + 10, 0);
	write_be32(ip + 12, IPV4_ADDRESS);
	write_be32(ip + 16, destination_ip);
	write_be16(ip + 10, checksum(ip, IPV4_HEADER_SIZE, 0));
	for (uint32_t index = 0; index < payload_length; index++) {
		ip[IPV4_HEADER_SIZE + index] = payload[index];
	}
	(void)e1000_send(frame, ETHERNET_HEADER_SIZE + ip_length);
}

static void handle_icmp(const uint8_t *frame, uint16_t length,
						uint32_t header_length, uint16_t ip_length) {
	uint8_t reply[NET_FRAME_MAX];
	const uint8_t *ip = frame + ETHERNET_HEADER_SIZE;
	uint32_t icmp_length = ip_length - header_length;
	uint8_t *icmp = reply + ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE;
	uint8_t destination_mac[6];

	if (icmp_length < 8 || ip[header_length] != 8) {
		return;
	}
	(void)length;
	for (uint32_t index = 0; index < 6; index++) {
		destination_mac[index] = frame[6 + index];
	}
	for (uint32_t index = 0; index < icmp_length; index++) {
		icmp[index] = frame[ETHERNET_HEADER_SIZE + header_length + index];
	}
	icmp[0] = 0;
	write_be16(icmp + 2, 0);
	write_be16(icmp + 2, checksum(icmp, icmp_length, 0));
	send_ipv4_packet(destination_mac, read_be32(ip + 12),
					 IP_PROTOCOL_ICMP, icmp, (uint16_t)icmp_length);
}

static uint32_t udp_checksum_initial(uint32_t source, uint32_t destination,
									uint16_t length) {
	return ((source >> 16) & 0xFFFFu) + (source & 0xFFFFu) +
		   ((destination >> 16) & 0xFFFFu) + (destination & 0xFFFFu) +
		   IP_PROTOCOL_UDP + length;
}

static void handle_udp(const uint8_t *ip, uint32_t header_length,
					   uint16_t ip_length) {
	const uint8_t *udp = ip + header_length;
	uint16_t udp_length;
	struct udp_entry *entry;

	if (ip_length < header_length + UDP_HEADER_SIZE) {
		return;
	}
	udp_length = read_be16(udp + 4);
	if (udp_length < UDP_HEADER_SIZE ||
		udp_length > ip_length - header_length ||
		udp_length - UDP_HEADER_SIZE > NET_UDP_PAYLOAD_MAX) {
		return;
	}
	if (read_be16(udp + 6) != 0 &&
		checksum(udp, udp_length,
				 udp_checksum_initial(read_be32(ip + 12),
									  read_be32(ip + 16),
									  udp_length)) != 0) {
		return;
	}
	entry = &udp_queue[udp_next];
	entry->packet.source_ipv4 = read_be32(ip + 12);
	entry->packet.source_port = read_be16(udp);
	entry->packet.destination_port = read_be16(udp + 2);
	entry->packet.length = udp_length - UDP_HEADER_SIZE;
	for (uint32_t index = 0; index < entry->packet.length; index++) {
		entry->packet.data[index] = udp[UDP_HEADER_SIZE + index];
	}
	entry->valid = 1;
	udp_next = (udp_next + 1) % UDP_QUEUE_SIZE;
}

static void handle_ipv4(const uint8_t *frame, uint16_t length) {
	const uint8_t *ip = frame + ETHERNET_HEADER_SIZE;
	uint32_t header_length;
	uint16_t ip_length;

	if (length < ETHERNET_HEADER_SIZE + IPV4_HEADER_SIZE ||
		(ip[0] >> 4) != 4) {
		return;
	}
	header_length = (ip[0] & 0x0Fu) * 4u;
	ip_length = read_be16(ip + 2);
	if (header_length < IPV4_HEADER_SIZE || ip_length < header_length ||
		ip_length > length - ETHERNET_HEADER_SIZE ||
		read_be32(ip + 16) != IPV4_ADDRESS ||
		checksum(ip, header_length, 0) != 0) {
		return;
	}
	if (ip[9] == IP_PROTOCOL_ICMP) {
		handle_icmp(frame, length, header_length, ip_length);
	} else if (ip[9] == IP_PROTOCOL_UDP) {
		handle_udp(ip, header_length, ip_length);
	}
}

static void dispatch_frame(const uint8_t *frame, uint16_t length) {
	uint16_t type;

	if (length < ETHERNET_HEADER_SIZE ||
		(!mac_equal(frame, local_mac) && frame[0] != 0xFF)) {
		return;
	}
	type = read_be16(frame + 12);
	if (type == ETHERTYPE_ARP) {
		handle_arp(frame, length);
	} else if (type == ETHERTYPE_IPV4) {
		handle_ipv4(frame, length);
	}
}

void net_poll(void) {
	uint8_t frame[NET_FRAME_MAX];
	int length;

	if (!device_ready) {
		return;
	}
	while ((length = e1000_receive(frame, sizeof(frame))) > 0) {
		uint32_t tail;
		struct raw_entry *entry;

		if (raw_count == RAW_QUEUE_SIZE) {
			raw_head = (raw_head + 1) % RAW_QUEUE_SIZE;
			raw_count--;
		}
		tail = (raw_head + raw_count) % RAW_QUEUE_SIZE;
		entry = &raw_queue[tail];
		entry->length = (uint16_t)length;
		for (uint32_t index = 0; index < (uint32_t)length; index++) {
			entry->data[index] = frame[index];
		}
		raw_count++;
		dispatch_frame(frame, (uint16_t)length);
	}
}

int net_init(void) {
	if (device_ready) {
		return 0;
	}
	if (e1000_init(local_mac) != 0) {
		return -1;
	}
	device_ready = 1;
	return 0;
}

int net_device_read(char *buffer, uint32_t size) {
	struct raw_entry *entry;

	if (buffer == 0 && size != 0) {
		return -1;
	}
	net_poll();
	if (!device_ready) {
		return -1;
	}
	if (raw_count == 0) {
		return -11;
	}
	entry = &raw_queue[raw_head];
	if (size < entry->length) {
		return -22;
	}
	for (uint32_t index = 0; index < entry->length; index++) {
		buffer[index] = (char)entry->data[index];
	}
	int length = entry->length;
	raw_head = (raw_head + 1) % RAW_QUEUE_SIZE;
	raw_count--;
	return length;
}

int net_device_write(const char *buffer, uint32_t size) {
	if (!device_ready || (buffer == 0 && size != 0) ||
		size > E1000_FRAME_MAX || size < ETHERNET_HEADER_SIZE) {
		return -1;
	}
	return e1000_send(buffer, (uint16_t)size);
}

int net_udp_send(uint32_t destination_ipv4, uint16_t source_port,
				 uint16_t destination_port, const void *data,
				 uint16_t length) {
	uint8_t destination_mac[6];
	uint32_t next_hop;
	uint8_t udp[UDP_HEADER_SIZE + NET_UDP_PAYLOAD_MAX];
	uint16_t udp_length = UDP_HEADER_SIZE + length;
	uint16_t sum;

	if (!device_ready || (data == 0 && length != 0) ||
		length > NET_UDP_PAYLOAD_MAX) {
		return -1;
	}
	next_hop = (destination_ipv4 & IPV4_NETMASK) ==
					   (IPV4_ADDRESS & IPV4_NETMASK)
				   ? destination_ipv4 : IPV4_GATEWAY;
	if (resolve_arp(next_hop, destination_mac) != 0) {
		return -11;
	}
	write_be16(udp, source_port);
	write_be16(udp + 2, destination_port);
	write_be16(udp + 4, udp_length);
	write_be16(udp + 6, 0);
	for (uint32_t index = 0; index < length; index++) {
		udp[UDP_HEADER_SIZE + index] = ((const uint8_t *)data)[index];
	}
	sum = checksum(udp, udp_length,
				   udp_checksum_initial(IPV4_ADDRESS, destination_ipv4,
										udp_length));
	write_be16(udp + 6, sum == 0 ? 0xFFFFu : sum);
	send_ipv4_packet(destination_mac, destination_ipv4, IP_PROTOCOL_UDP,
					 udp, udp_length);
	net_poll();
	return length;
}

int net_udp_receive(uint16_t destination_port, struct net_udp_packet *packet) {
	if (!device_ready || packet == 0) {
		return -1;
	}
	net_poll();
	for (uint32_t index = 0; index < UDP_QUEUE_SIZE; index++) {
		if (udp_queue[index].valid &&
			udp_queue[index].packet.destination_port == destination_port) {
			*packet = udp_queue[index].packet;
			udp_queue[index].valid = 0;
			return packet->length;
		}
	}
	return -11;
}

uint32_t net_ipv4_address(void) {
	return IPV4_ADDRESS;
}
