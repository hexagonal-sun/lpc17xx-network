#include "ipv4.h"
#include "byteswap.h"
#include "arp.h"
#include "ethernet.h"
#include "memory.h"
#include "udp.h"
#include "tcp.h"
#include "process.h"
#include "atomics.h"
#include "irq.h"
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
static mutex_t ip4_rx_queue_lock = 0;

static LIST(ip4_tx_queue);
static mutex_t ip4_tx_queue_lock = 0;

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

    void *our_packet = get_mem(packet_len);
    memcpy(our_packet, packet, packet_len);

    new_packet->packet = our_packet;
    new_packet->packet_len = packet_len;

    get_lock(&ip4_rx_queue_lock);
    list_add(&new_packet->packets, &ip4_rx_queue);
    release_lock(&ip4_rx_queue_lock);
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

    new_packet->protocol = protocol;
    new_packet->dst_ip = dst_ip;
    new_packet->payload = payload;
    new_packet->payload_len = payload_len;

    get_lock(&ip4_tx_queue_lock);
    list_add(&new_packet->next, &ip4_tx_queue);
    release_lock(&ip4_tx_queue_lock);
}

static void ip4_do_xmit_packet(uint8_t protocol, uint32_t dst_ip, void *payload,
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

    ether_tx(dst_hw_addr, ETHERTYPE_IP, packet_buf,
                       packet_buf_len);
}

static void ip4_task(void)
{
    while (1) {
        ip4_rx_q_t *rx_packet;
        ip4_tx_q_t *tx_packet;

        do {
            __irq_disable();
            if (try_lock(&ip4_rx_queue_lock)) {
                list_pop(rx_packet, &ip4_rx_queue, packets);

                release_lock(&ip4_rx_queue_lock);
            } else
                rx_packet = NULL;
            __irq_enable();

            if (rx_packet) {
                ip4_do_rx_packet(rx_packet->packet, rx_packet->packet_len);

                free_mem(rx_packet->packet);
                free_mem(rx_packet);
            }
        } while (rx_packet);

        do {
            __irq_disable();
            if (try_lock(&ip4_tx_queue_lock)) {
                list_pop(tx_packet, &ip4_tx_queue, next);

                release_lock(&ip4_tx_queue_lock);
            } else
                tx_packet = NULL;
            __irq_enable();

            if (tx_packet) {

                ip4_do_xmit_packet(tx_packet->protocol, tx_packet->dst_ip,
                                   tx_packet->payload, tx_packet->payload_len);

                free_mem(tx_packet->payload);
                free_mem(tx_packet);
            }
        } while (tx_packet);
    }
}
thread(ip4_task);
