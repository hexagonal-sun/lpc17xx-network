#pragma once
#include <stdint.h>

#define IP_PROTO_TCP 0x6        /* Ambitious! */
#define IP_PROTO_UDP 0x11

typedef struct {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t __reserved_0;
    uint16_t tot_length;
    uint16_t identification;
    uint16_t __reserverd_1;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip4_header;

void ip4_rx_packet(void *packet, int packet_len);

void ip4_xmit_packet(uint8_t protocol, uint32_t dst_ip, void *payload,
                     uint16_t payload_len);
