#include "lpc17xx.h"
#include "arp.h"
#include "memory.h"
#include "byteswap.h"
#include "emac.h"
#include <string.h>

void ether_rx_frame(void *frame, int frame_len)
{
    ethernet_header *header = frame;
    swap_endian16(&header->ether_type);
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
