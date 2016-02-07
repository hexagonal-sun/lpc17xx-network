#include "memory.h"
#include "arp.h"
#include "byteswap.h"
#include "emac.h"

struct arp_entry
{
    uint8_t ether_addr[ETHER_ADDR_LEN];
    uint32_t ipaddr;
    struct arp_entry *next;
};

struct arp_entry *arp_table = 0;

static void arp_swap_endian(arp_packet *packet)
{
    swap_endian16(&packet->HTYPE);
    swap_endian16(&packet->PTYPE);
    swap_endian16(&packet->OPER);
    swap_endian32(&packet->SPA);
    swap_endian32(&packet->TPA);
}

uint8_t * resolve_address(uint32_t ip_address)
{
    int i;
    struct arp_entry *cur_arp_entry = arp_table;
    uint8_t broadcast_addr[ETHER_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    while (cur_arp_entry)
        if (cur_arp_entry->ipaddr == ip_address)
            return cur_arp_entry->ether_addr;

    /* Need to send out ARP packet to resolve address. */
    arp_packet *arp_request = (arp_packet *)get_mem(sizeof(arp_packet));

    /* Fill in ARP request fields. */
    arp_request->HTYPE = HTYPE_ETHERNET;
    arp_request->PTYPE = ETHERTYPE_IP;
    arp_request->HLEN  = 6;
    arp_request->PLEN  = 4;
    arp_request->OPER  = OPER_REQUEST;

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        arp_request->SHA[i] = ether_addr[i];
        arp_request->THA[i] = 0x0;     /* Ignored on ARP requests. */
    }

    arp_request->SPA = OUR_IP_ADDRESS;
    arp_request->TPA = ip_address;

    arp_swap_endian(arp_request);

    ether_xmit_payload(broadcast_addr, ETHERTYPE_ARP, arp_request,
                       sizeof(*arp_request));

    free_mem(arp_request);

    return 0;
}
