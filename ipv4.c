#include "ipv4.h"
#include "byteswap.h"
#include "arp.h"
#include "ethernet.h"
#include "memory.h"
#include "udp.h"
#include "tcp.h"
#include <string.h>

#define DEFAULT_TTL 10

static void ip4_swap_endian(ip4_header *iphdr)
{
    swap_endian16(&iphdr->tot_length);
    swap_endian16(&iphdr->identification);
    swap_endian32(&iphdr->src_ip);
    swap_endian32(&iphdr->dst_ip);
}

static void ip4_compute_checksum(ip4_header *header)
{
    uint16_t *x = (uint16_t *)header;
    uint32_t sum = 0;
    int i;

    for (i = 0; i < sizeof(ip4_header) / 2; i++)
        sum += x[i];

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    header->header_checksum = ~sum;
}

void ip4_rx_packet(void *packet, int packet_len)
{
    ip4_header *header = (ip4_header *)packet;
    void *payload;
    int payload_len, header_len;

    ip4_swap_endian(header);

    /* TODO: checksum checking. */

    header_len = header->ihl * 4;
    payload = header + header_len;
    payload_len = header->tot_length - header_len;

    /* Drop packet if ttl is zero. */
    if (!header->ttl)
        return;

    /* Drop packet if it is not addressed to us. */
    if (header->dst_ip != OUR_IP_ADDRESS)
        return;

    switch (header->protocol)
    {
    case IP_PROTO_TCP:
        tcp_rx_packet(header->src_ip, payload, payload_len);
        break;
    case IP_PROTO_UDP:
        udp_rx_packet(header->src_ip, payload, payload_len);
        break;
    default:
        return;
    }
}

void ip4_xmit_packet(uint8_t protocol, uint32_t dst_ip, void *payload,
                     uint16_t payload_len)
{
    void *packet_buf;
    int packet_buf_len = sizeof(ip4_header) + payload_len;
    ip4_header *header;
    uint8_t *dst_hw_addr = resolve_address(dst_ip);

    if (!dst_hw_addr)
        return;

    packet_buf = get_mem(packet_buf_len);
    memset(packet_buf, 0, packet_buf_len);

    header = (ip4_header *)packet_buf;

    header->version = 4;
    header->ihl = 5;
    header->tot_length = packet_buf_len;
    header->ttl = DEFAULT_TTL;
    header->protocol = protocol;
    header->src_ip = OUR_IP_ADDRESS;
    header->dst_ip = dst_ip;

    ip4_swap_endian(header);

    ip4_compute_checksum(header);

    memcpy(packet_buf + sizeof(*header), payload, payload_len);

    ether_xmit_payload(dst_hw_addr, ETHERTYPE_IP, packet_buf,
                       packet_buf_len);

    free_mem(header);
}
