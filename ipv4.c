#include "ipv4.h"
#include "byteswap.h"
#include "arp.h"
#include "ethernet.h"
#include "init.h"
#include "protocol.h"
#include "memory.h"
#include "process.h"
#include "irq.h"
#include "wait.h"
#include <string.h>

#define DEFAULT_TTL 10

typedef struct
{
    uint8_t protocol;
    uint32_t dst_ip;
    void *payload;
    uint16_t payload_len;
    list next;
} ip4_tx_q_t;

static LIST(ip4_tx_queue);
static WAITQUEUE(ip4_tx_waitq);

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

static void ipv4_rx_packet(struct packet_t *pkt)
{
    ip4_header *header = (ip4_header *)pkt->cur_data;
    void *payload;
    size_t payload_len, header_len;

    ip4_swap_endian(header);

    /* TODO: checksum checking. */

    header_len = header->ihl * 4;
    pkt->cur_data += header_len;
    pkt->cur_data_length -= header_len;

    /* Drop packet if ttl is zero. */
    if (!header->ttl) {
        pkt->handler = DROP;
        return;
    }

    /* Drop packet if it is not addressed to us. */
    if (header->dst_ip != OUR_IP_ADDRESS) {
        pkt->handler = DROP;
        return;
    }

    pkt->ip4_info.dst_ip = header->dst_ip;
    pkt->ip4_info.src_ip = header->src_ip;

    switch (header->protocol)
    {
    case IP_PROTO_TCP:
        pkt->handler = TCP;
        break;
    case IP_PROTO_UDP:
        pkt->handler = UDP;
        break;
    default:
        pkt->handler = DROP;
        return;
    }
}

void ip4_xmit_packet(uint8_t protocol, uint32_t dst_ip, void *payload,
                     uint16_t payload_len)
{
    ip4_tx_q_t *new_packet = get_mem(sizeof(*new_packet));
    void *payload_copy = get_mem(payload_len);
    irq_flags_t flags;

    memcpy(payload_copy, payload, payload_len);

    new_packet->protocol = protocol;
    new_packet->dst_ip = dst_ip;
    new_packet->payload = payload_copy;
    new_packet->payload_len = payload_len;

    flags = irq_disable();
    list_add(&new_packet->next, &ip4_tx_queue);
    irq_enable(flags);

    waitqueue_wakeup(&ip4_tx_waitq);
}

static uint32_t ip4_get_pkt_dst(uint32_t dst_ip)
{
    if ((dst_ip & NET_MASK) == (OUR_IP_ADDRESS & NET_MASK))
        return dst_ip;

    return IP_GATEWAY;
}

static void ip4_do_xmit_packet(uint8_t protocol, uint32_t dst_ip, void *payload,
                               uint16_t payload_len)
{
    void *packet_buf;
    int packet_buf_len = sizeof(ip4_header) + payload_len;
    ip4_header *header;
    uint32_t pkt_dst_ip = ip4_get_pkt_dst(dst_ip);
    uint8_t *dst_hw_addr = resolve_address(pkt_dst_ip);

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

    ether_tx(dst_hw_addr, ETHERTYPE_IP, packet_buf,
                       packet_buf_len);

    free_mem(packet_buf);
}

static void ip4_tx_task(void)
{
    while (1) {
        ip4_tx_q_t *tx_pkt;
        irq_flags_t flags;

        wait_for_volatile_condition((!list_empty(&ip4_tx_queue)),
                                    ip4_tx_waitq);

        flags = irq_disable();
        list_pop(tx_pkt, &ip4_tx_queue, next);
        irq_enable(flags);

        if (tx_pkt) {
            ip4_do_xmit_packet(tx_pkt->protocol, tx_pkt->dst_ip,
                               tx_pkt->payload,  tx_pkt->payload_len);

            free_mem(tx_pkt->payload);
            free_mem(tx_pkt);
        }
    }
}
thread(ip4_tx_task);

static struct protocol_t ipv4_protocol = {
    .rx_pkt = ipv4_rx_packet,
    .type = IPV4
};

static void ipv4_init(void)
{
    protocol_register(&ipv4_protocol);
}
initcall(ipv4_init);
