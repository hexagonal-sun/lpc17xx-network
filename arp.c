#include "memory.h"
#include "arp.h"
#include "byteswap.h"
#include "emac.h"
#include "list.h"

struct arp_entry
{
    uint8_t ether_addr[ETHER_ADDR_LEN];
    uint32_t ipaddr;
    list arp_table;
};

LIST(arp_table_head);

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
    struct arp_entry *cur;
    uint8_t broadcast_addr[ETHER_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    list_for_each(cur, &arp_table_head, arp_table) {
        if (cur->ipaddr == ip_address)
            return cur->ether_addr;
    }

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

void arp_process_packet(void *payload, int payload_len)
{
    arp_packet *packet = payload;

    arp_swap_endian(packet);

    /* We are only interested in Ethernet type ARP packets. */
    if (packet->HTYPE != HTYPE_ETHERNET)
        return;

    switch (packet->OPER)
    {
    case OPER_REQUEST:
        /* TODO: Send an appropriate ARP reply if we are the subject
         * of the request. */
        return;
    case OPER_REPLY:
    {
        struct arp_entry *new_arp_entry;

        /* Ensure this packet is for us. */
        if (!ethernet_mac_equal(ether_addr, packet->THA))
            return;



    }
    default:
        return;
    }
}
