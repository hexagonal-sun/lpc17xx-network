#include "lpc17xx.h"
#include "arp.h"
#include "atomics.h"
#include "memory.h"
#include "byteswap.h"
#include "emac.h"
#include "ipv4.h"
#include "list.h"
#include "irq.h"
#include "tick.h"
#include "init.h"
#include "process.h"
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

    void *our_frame = get_mem(frame_len);
    memcpy(our_frame, frame, frame_len);

    newPacket->packet = our_frame;
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

static void ether_task(void)
{
    while (1) {
        struct ether_rx_q_t *rx_packet;
        struct ether_tx_q_t *tx_packet;

        do {
            __irq_disable();
            if (try_lock(&ether_rx_queue_lock)) {
                list_pop(rx_packet, &ether_rx_queue, packets);

                release_lock(&ether_rx_queue_lock);
            } else
                rx_packet = NULL;
            __irq_enable();

            if (rx_packet) {
                ether_process_frame(rx_packet->packet, rx_packet->packet_len);

                free_mem(rx_packet->packet);
                free_mem(rx_packet);
            }
        } while (rx_packet);

        do {
            __irq_disable();
            if (try_lock(&ether_tx_queue_lock)) {
                list_pop(tx_packet, &ether_tx_queue, next);

                release_lock(&ether_tx_queue_lock);
            } else
                tx_packet = NULL;
            __irq_enable();

            if (tx_packet) {

                ether_xmit_payload(tx_packet->dhost, tx_packet->ether_type,
                                   tx_packet->payload, tx_packet->payload_len);

                free_mem(tx_packet->payload);
                free_mem(tx_packet);
            }
        } while (tx_packet);
    }
}
thread(ether_task);
