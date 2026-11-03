#ifndef DRIVER_NET_H
#define DRIVER_NET_H

#include <stdint.h>

#define NET_FRAME_MAX 1518u
#define NET_UDP_PAYLOAD_MAX 1472u

struct net_udp_packet {
	uint32_t source_ipv4;
	uint16_t source_port;
	uint16_t destination_port;
	uint16_t length;
	uint8_t data[NET_UDP_PAYLOAD_MAX];
};

struct net_udp_send_request {
	uint32_t destination_ipv4;
	uint16_t source_port;
	uint16_t destination_port;
	const void *data;
	uint16_t length;
};

int net_init(void);
void net_poll(void);
int net_device_read(char *buffer, uint32_t size);
int net_device_write(const char *buffer, uint32_t size);
int net_udp_send(uint32_t destination_ipv4, uint16_t source_port,
				 uint16_t destination_port, const void *data,
				 uint16_t length);
int net_udp_receive(uint16_t destination_port, struct net_udp_packet *packet);
uint32_t net_ipv4_address(void);

#endif
