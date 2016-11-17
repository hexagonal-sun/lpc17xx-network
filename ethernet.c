#include "lpc17xx.h"
#include "arp.h"
#include "atomics.h"
#include "memory.h"
#include "byteswap.h"
#include "emac.h"
#include "ipv4.h"
#include "list.h"
#include "tick.h"
#include "init.h"
#include <string.h>

struct ether_rx_q_t
{
    void *packet;
    uint16_t packet_len;
    list packets;
};

struct ether_tx_q_t
{
    uint8_t dhost[ETHER_ADDR_LEN];
    uint16_t ether_type;
    void *payload;
    uint16_t payload_len;
    list next;
};

LIST(ether_rx_queue);
static mutex_t ether_rx_queue_lock = 0;

LIST(ether_tx_queue);
static mutex_t ether_tx_queue_lock = 0;

void ether_rx_frame(void *frame, int frame_len)
{
    struct ether_rx_q_t *newPacket = get_mem(sizeof(*newPacket));

    newPacket->packet = frame;
    newPacket->packet_len = frame_len;

    get_lock(&ether_rx_queue_lock);
    list_add(&newPacket->packets, &ether_rx_queue);
    release_lock(&ether_rx_queue_lock);
}

void ether_tx(uint8_t dhost[ETHER_ADDR_LEN], uint16_t ether_type,
              void *payload, int len)
{
    struct ether_tx_q_t *newPacket = get_mem(sizeof(*newPacket));

    ethernet_mac_copy(newPacket->dhost, dhost);
    newPacket->ether_type = ether_type;
    newPacket->payload = payload;
    newPacket->payload_len = len;

    get_lock(&ether_tx_queue_lock);
    list_add(&newPacket->next, &ether_tx_queue);
    release_lock(&ether_tx_queue_lock);
}


static void ether_process_frame(void *frame, int frame_len)
{
    static int no_dropped_packets;
    ethernet_header *header = frame;
    void *payload = frame + sizeof(*header);
    int payload_len = frame_len - sizeof(*header);

    swap_endian16(&header->ether_type);

    switch (header->ether_type)
    {
    case ETHERTYPE_ARP:
        arp_process_packet(payload, payload_len);
        break;
    case ETHERTYPE_IP:
        ip4_rx_packet(payload, payload_len);
        break;
    default:
        /* Drop the packet. */
        no_dropped_packets++;
    }

    free_mem(frame);
}

static void ether_xmit_payload(uint8_t dhost[ETHER_ADDR_LEN],
                               uint16_t ether_type, void *payload, int len)
{
    int i;
    ethernet_header *header = (ethernet_header *)get_mem(sizeof(ethernet_header));

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        header->ether_dhost[i] = dhost[i];
        header->ether_shost[i] = ether_addr[i];
    }

    header->ether_type = ether_type;
    swap_endian16(&header->ether_type);

    emac_xmit_frame(header, payload, len);

    free_mem(header);
}

int ethernet_mac_equal(uint8_t *a, uint8_t *b)
{
    if (memcmp(a, b, ETHER_ADDR_LEN))
        return 0;
    else
        return 1;
}

void ethernet_mac_copy(uint8_t *dst, uint8_t *src)
{
    memcpy(dst, src, ETHER_ADDR_LEN);
}

static void ether_tick()
{
    list *i, *tmp;

    if (try_lock(&ether_rx_queue_lock))
    {
        list_for_each_safe(i, tmp, &ether_rx_queue)
        {
            struct ether_rx_q_t *packet =
                list_entry(i, struct ether_rx_q_t, packets);

            ether_process_frame(packet->packet, packet->packet_len);

            list_del(i);
            free_mem(packet);
            free_mem(i);
        }
        release_lock(&ether_rx_queue_lock);
    }

    if (try_lock(&ether_tx_queue_lock))
    {
        list_for_each_safe(i, tmp, &ether_tx_queue)
        {
            struct ether_tx_q_t *packet =
                list_entry(i, struct ether_tx_q_t, next);

            ether_xmit_payload(packet->dhost, packet->ether_type,
                               packet->payload, packet->payload_len);

            list_del(i);
            free_mem(packet->payload);
            free_mem(packet);
            free_mem(i);
        }
        release_lock(&ether_tx_queue_lock);
    }
}

static struct tick_work_q ether_tick_work = {
    .tick_fn = ether_tick
};

void ether_init(void)
{
    tick_add_work_fn(&ether_tick_work);
}
initcall(ether_init);
