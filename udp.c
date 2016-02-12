#include "udp.h"
#include "ipv4.h"
#include "byteswap.h"
#include "memory.h"
#include <string.h>

static void udp_swap_endian(udp_header *header)
{
    swap_endian16(&header->dst_port);
    swap_endian16(&header->length);
}

void udp_xmit_packet(uint16_t dst_port, uint32_t dst_ip, void *payload,
                     int payload_len)
{
    int packet_buf_len = sizeof(udp_header) + payload_len;
    void *packet_buf = get_mem(packet_buf_len);
    udp_header *header = (udp_header *)packet_buf;

    header->src_port = 0;
    header->dst_port = dst_port;
    header->length = packet_buf_len;
    header->checksum = 0;

    udp_swap_endian(header);

    memcpy(packet_buf + sizeof(*header), payload, payload_len);

    ip4_xmit_packet(IP_PROTO_UDP, dst_ip, packet_buf, packet_buf_len);

    free_mem(packet_buf);
}
