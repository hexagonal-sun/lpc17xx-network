#include "lpc17xx.h"
#include "arp.h"
#include "memory.h"
#include "byteswap.h"
#include "emac.h"
#include "ipv4.h"
#include "list.h"
#include "irq.h"
#include "tick.h"
#include "init.h"
#include "process.h"
#include "wait.h"
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

static LIST(ether_rx_queue);
static WAITQUEUE(ether_rx_waitq);

static LIST(ether_tx_queue);
static WAITQUEUE(ether_tx_waitq);

void ether_rx_frame(void *frame, int frame_len)
{
    struct ether_rx_q_t *newPacket = get_mem(sizeof(*newPacket));
    irq_flags_t flags;

    void *our_frame = get_mem(frame_len);
    memcpy(our_frame, frame, frame_len);

    newPacket->packet = our_frame;
    newPacket->packet_len = frame_len;

    flags = irq_disable();
    list_add(&newPacket->packets, &ether_rx_queue);
    irq_enable(flags);

    waitqueue_wakeup(&ether_rx_waitq);
}

void ether_tx(uint8_t dhost[ETHER_ADDR_LEN], uint16_t ether_type,
              void *payload, int len)
{
    struct ether_tx_q_t *newPacket = get_mem(sizeof(*newPacket));
    void *payload_copy = get_mem(len);
    irq_flags_t flags;

    memcpy(payload_copy, payload, len);

    ethernet_mac_copy(newPacket->dhost, dhost);
    newPacket->ether_type = ether_type;
    newPacket->payload = payload_copy;
    newPacket->payload_len = len;

    flags = irq_disable();
    list_add(&newPacket->next, &ether_tx_queue);
    irq_enable(flags);

    waitqueue_wakeup(&ether_tx_waitq);
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

static void ether_rx_task(void)
{
    while (1) {
        struct ether_rx_q_t *rxd_pkt;
        irq_flags_t flags;

        wait_for_volatile_condition((!list_empty(&ether_rx_queue)),
                                    ether_rx_waitq);

        flags = irq_disable();
        list_pop(rxd_pkt, &ether_rx_queue, packets);
        irq_enable(flags);

        if (rxd_pkt) {
            ether_process_frame(rxd_pkt->packet, rxd_pkt->packet_len);

            free_mem(rxd_pkt->packet);
            free_mem(rxd_pkt);
        }
    }
}
thread(ether_rx_task);

static void ether_tx_task(void)
{
    while (1) {
        struct ether_tx_q_t *txd_pkt;
        irq_flags_t flags;

        wait_for_volatile_condition((!list_empty(&ether_tx_queue)),
                                    ether_tx_waitq);

        flags = irq_disable();
        list_pop(txd_pkt, &ether_tx_queue, next);
        irq_enable(flags);

        if (txd_pkt) {
            ether_xmit_payload(txd_pkt->dhost, txd_pkt->ether_type,
                               txd_pkt->payload, txd_pkt->payload_len);

            free_mem(txd_pkt->payload);
            free_mem(txd_pkt);
        }
    }
}
thread(ether_tx_task);
