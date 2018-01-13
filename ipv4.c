#include "ipv4.h"
#include "byteswap.h"
#include "arp.h"
#include "ethernet.h"
#include "memory.h"
#include "udp.h"
#include "tcp.h"
#include "process.h"
#include "irq.h"
#include "wait.h"
#include <string.h>

#define DEFAULT_TTL 10

typedef struct
{
    void *packet;
    uint16_t packet_len;
    list packets;
} ip4_rx_q_t;

typedef struct
{
    uint8_t protocol;
    uint32_t dst_ip;
    void *payload;
    uint16_t payload_len;
    list next;
} ip4_tx_q_t;

static LIST(ip4_rx_queue);
static WAITQUEUE(ip4_rx_waitq);

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

void ip4_rx_packet(void *packet, int packet_len)
{
    ip4_rx_q_t *new_packet = get_mem(sizeof(*new_packet));
    irq_flags_t flags;

    void *our_packet = get_mem(packet_len);
    memcpy(our_packet, packet, packet_len);

    new_packet->packet = our_packet;
    new_packet->packet_len = packet_len;

    flags = irq_disable();
    list_add(&new_packet->packets, &ip4_rx_queue);
    irq_enable(flags);

    waitqueue_wakeup(&ip4_rx_waitq);
}

static void ip4_do_rx_packet(void *packet, int packet_len)
{
    ip4_header *header = (ip4_header *)packet;
    void *payload;
    int payload_len, header_len;

    ip4_swap_endian(header);

    /* TODO: checksum checking. */

    header_len = header->ihl * 4;
    payload = packet + header_len;
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
    ip4_tx_q_t *new_packet = get_mem(sizeof(*new_packet));
    irq_flags_t flags;

    new_packet->protocol = protocol;
    new_packet->dst_ip = dst_ip;
    new_packet->payload = payload;
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
}

static void ip4_rx_task(void)
{
    while (1) {
        ip4_rx_q_t *rxd_pkt;
        irq_flags_t flags;

        wait_for_volatile_condition((!list_empty(&ip4_rx_queue)),
                                    ip4_rx_waitq);

        flags = irq_disable();
        list_pop(rxd_pkt, &ip4_rx_queue, packets);
        irq_enable(flags);

        if (rxd_pkt) {
            ip4_do_rx_packet(rxd_pkt->packet, rxd_pkt->packet_len);

            free_mem(rxd_pkt->packet);
            free_mem(rxd_pkt);
        }
    }
}
thread(ip4_rx_task)

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
thread(ip4_tx_task)
