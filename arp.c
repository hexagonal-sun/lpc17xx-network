#include "memory.h"
#include "arp.h"
#include "byteswap.h"
#include "protocol.h"
#include "emac.h"
#include "tick.h"
#include "list.h"
#include "init.h"
#include "wait.h"
#include <string.h>

#define ARP_TIMEOUT 250

static WAITQUEUE(arp_waitqueue);

struct arp_entry
{
    uint8_t ether_addr[ETHER_ADDR_LEN];
    uint32_t ipaddr;
    list arp_table;
};

struct arp_pending_request
{
    int tick_count;
    int TPA;
    int timed_out;
    int finished;
    struct arp_entry *answer;
    list requests;
};

static LIST(arp_pending_requests);
static LIST(arp_table_head);

static void arp_tick()
{
    list *i, *tmp;
    irq_flags_t flags = irq_disable();

    list_for_each_safe(i, tmp, &arp_pending_requests) {
        struct arp_pending_request *arp_req =
            list_entry(i, struct arp_pending_request, requests);

        if (arp_req->tick_count++ >= ARP_TIMEOUT) {
            arp_req->finished = 1;
            arp_req->timed_out = 1;
            list_del(i);
        }
    }

    irq_enable(flags);
}

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
    struct arp_pending_request arp_p_req;
    uint8_t broadcast_addr[ETHER_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    irq_flags_t flags;

    flags = irq_disable();
    list_for_each(cur, &arp_table_head, arp_table) {
        if (cur->ipaddr == ip_address)
            return cur->ether_addr;
    }
    irq_enable(flags);

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

    memset(&arp_p_req, 0, sizeof(arp_p_req));
    arp_p_req.TPA = ip_address;

    flags = irq_disable();
    list_add(&arp_p_req.requests, &arp_pending_requests);
    irq_enable(flags);

    arp_swap_endian(arp_request);

    ether_tx(broadcast_addr, ETHERTYPE_ARP, arp_request,
             sizeof(*arp_request));

    wait_for_volatile_condition(arp_p_req.finished, arp_waitqueue);

    if (arp_p_req.timed_out)
        return 0;

    return arp_p_req.answer->ether_addr;
}

static void arp_rx_packet(struct packet_t *pkt)
{
    arp_packet *packet = (arp_packet *)pkt->cur_data;

    /* This function will process any arp packets; they're not passed
     * to any other prtocol layers.  Therefore, we can drop this
     * packet once we have finished processing it. */
    pkt->handler = DROP;

    arp_swap_endian(packet);

    /* We are only interested in Ethernet type ARP packets. */
    if (packet->HTYPE != HTYPE_ETHERNET)
        return;

    switch (packet->OPER)
    {
    case OPER_REQUEST:
    {
        arp_packet *resp;
        int i;

        if (packet->TPA != OUR_IP_ADDRESS)
            return;

        resp = get_mem(sizeof(*resp));

        resp->HTYPE = HTYPE_ETHERNET;
        resp->PTYPE = ETHERTYPE_IP;
        resp->HLEN  = 6;
        resp->PLEN  = 4;
        resp->OPER  = OPER_REPLY;

        for (i = 0; i < ETHER_ADDR_LEN; i++) {
            resp->SHA[i] = ether_addr[i];
            resp->THA[i] = packet->SHA[i];
        }

        resp->SPA = OUR_IP_ADDRESS;
        resp->TPA = packet->SPA;

        arp_swap_endian(resp);

        ether_tx(packet->SHA, ETHERTYPE_ARP, resp, sizeof(*resp));
        break;
    }
    case OPER_REPLY:
    {
        struct arp_entry *new_arp_entry;
        list *i, *tmp;
        irq_flags_t flags;

        /* Ensure this packet is for us. */
        if (!ethernet_mac_equal(ether_addr, packet->THA))
            return;

        flags = irq_disable();

        /* Find the request that this packet fulfils */
        list_for_each_safe(i, tmp, &arp_pending_requests) {
            struct arp_pending_request *arp_req =
                list_entry(i, struct arp_pending_request, requests);

            if (arp_req->TPA == packet->SPA) {

                new_arp_entry = get_mem(sizeof(*new_arp_entry));

                ethernet_mac_copy(new_arp_entry->ether_addr, packet->SHA);
                new_arp_entry->ipaddr = packet->SPA;

                list_add(&new_arp_entry->arp_table, &arp_table_head);

                /* We have fulfilled the request, set the reply and remove
                 * from the list of pending requests. */
                arp_req->answer = new_arp_entry;
                arp_req->finished = 1;

                list_del(i);
                break;
            }
        }

        waitqueue_wakeup(&arp_waitqueue);
        irq_enable(flags);
    }
    default:
        return;
    }
}

struct protocol_t arp_protocol = {
    .type = ARP,
    .rx_pkt = arp_rx_packet
};

struct tick_work_q arp_tick_work = {
    .tick_fn = arp_tick
};

static void arp_init(void)
{
    tick_add_work_fn(&arp_tick_work);
    protocol_register(&arp_protocol);
}
initcall(arp_init);
