#ifndef USER_NET_H
#define USER_NET_H

#include <stdint.h>
#include <driver/net.h>
#include <user/syscall.h>

static inline int32_t ok_net_udp_send(uint32_t destination_ipv4,
									  uint16_t source_port,
									  uint16_t destination_port,
									  const void *data, uint16_t length) {
	struct net_udp_send_request request;

	request.destination_ipv4 = destination_ipv4;
	request.source_port = source_port;
	request.destination_port = destination_port;
	request.data = data;
	request.length = length;
	return ok_syscall3(OK_SYS_NET_UDP_SEND,
					   (uint32_t)(uintptr_t)&request, 0, 0);
}

static inline int32_t ok_net_udp_receive(uint16_t destination_port,
										 struct net_udp_packet *packet) {
	if (packet == 0) {
		return -OK_EFAULT;
	}
	return ok_syscall3(OK_SYS_NET_UDP_RECEIVE, destination_port,
					   (uint32_t)(uintptr_t)packet, 0);
}

#endif
