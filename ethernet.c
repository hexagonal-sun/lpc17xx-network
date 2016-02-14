#include "lpc17xx.h"
#include "arp.h"
#include "memory.h"
#include "byteswap.h"
#include "emac.h"
#include "ipv4.h"
#include <string.h>

void ether_rx_frame(void *frame, int frame_len)
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

void ether_xmit_payload(uint8_t dhost[ETHER_ADDR_LEN], uint16_t ether_type,
                        void *payload, int len)
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
