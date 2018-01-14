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
#include "protocol.h"
#include "wait.h"
#include <string.h>

struct ether_tx_q_t
{
    uint8_t dhost[ETHER_ADDR_LEN];
    uint16_t ether_type;
    void *payload;
    uint16_t payload_len;
    list next;
};

static LIST(ether_tx_queue);
static WAITQUEUE(ether_tx_waitq);

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

static void ethernet_rx_pkt(struct packet_t *pkt)
{
    static int no_dropped_packets;
    ethernet_header *header = (ethernet_header *)pkt->cur_data;
    pkt->cur_data += sizeof(*header);
    pkt->cur_data_length -= sizeof(*header);

    swap_endian16(&header->ether_type);

    switch (header->ether_type)
    {
    case ETHERTYPE_ARP:
        pkt->handler = ARP;
        break;
    case ETHERTYPE_IP:
        pkt->handler = IPV4;
        break;
    default:
        /* Drop the packet. */
        pkt->handler = DROP;
        no_dropped_packets++;
        break;
    }
}

struct protocol_t ethernet_protocol = {
    .type = ETHERNET,
    .rx_pkt = ethernet_rx_pkt
};

void ethernet_init(void)
{
    protocol_register(&ethernet_protocol);
}
initcall(ethernet_init);

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
