#ifndef SDRIVERS_NET_E1000_H
#define SDRIVERS_NET_E1000_H

#include <stdint.h>

#define E1000_FRAME_MAX 1518u

int e1000_init(uint8_t mac_address[6]);
int e1000_send(const void *frame, uint16_t length);
int e1000_receive(void *frame, uint16_t capacity);

#endif
