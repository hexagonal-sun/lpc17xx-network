#pragma once
#include <stdint.h>

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_header;

int udp_rx(uint16_t port, void *dst_buf, uint16_t dst_buf_sz);

void udp_xmit_packet(uint16_t dst_port, uint32_t dst_ip, void *payload,
                     int payload_len);
