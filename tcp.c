#include "tcp.h"
#include "byteswap.h"
#include "ipv4.h"
#include "ethernet.h"
#include "memory.h"
#include "tick.h"
#include "init.h"
#include "wait.h"
#include <string.h>

#define TCP_TIMEOUT 250

typedef struct
{
    uint32_t src;
    uint32_t dst;
    uint8_t __reserved_0;
    uint8_t proto;
    uint16_t length;
} tcp_pseudo;


LIST(tcb_head);

static void tcp_swap_endian(tcp_header *header)
{
    swap_endian16(&header->source_port);
    swap_endian16(&header->dest_port);
    swap_endian32(&header->seq_n);
    swap_endian32(&header->ack_n);
    swap_endian16(&header->window_sz);
    swap_endian16(&header->checksum);
    swap_endian16(&header->urg_ptr);
}

static void tcp_swap_pseudo_endian(tcp_pseudo *pheader)
{
    swap_endian32(&pheader->src);
    swap_endian32(&pheader->dst);
    swap_endian16(&pheader->length);
}

static void tcp_compute_checksum(tcp_header *header, tcp_pseudo *pheader)
{
    uint16_t *x = (uint16_t *)header;
    uint16_t *px = (uint16_t *)pheader;
    uint32_t sum = 0;
    int i;

    for (i = 0; i < sizeof(tcp_pseudo) / 2; i++)
        sum += px[i];

    for (i = 0; i < sizeof(tcp_header) / 2; i++)
        sum += x[i];

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    header->checksum = ~sum;
}

static void tcp_header_prepopulate(tcb *t, tcp_header *header)
{
    header->source_port = t->src_port;
    header->dest_port = t->dst_port;
    header->seq_n = t->cur_seq_n;
    header->ack_n = t->cur_ack_n;
    header->data_offset = 5;

    /* TODO: keep track of the rx buffer and hence the window_sz. */
    header->window_sz = 10;
}

static void tcp_tx(tcp_header header, uint32_t dest_ip)
{
    tcp_pseudo pheader;

    memset(&pheader, 0, sizeof(pheader));

    pheader.src = OUR_IP_ADDRESS;
    pheader.dst = dest_ip;
    pheader.proto = IP_PROTO_TCP;
    pheader.length = sizeof(header);

    tcp_swap_endian(&header);
    tcp_swap_pseudo_endian(&pheader);

    tcp_compute_checksum(&header, &pheader);

    ip4_xmit_packet(IP_PROTO_TCP, dest_ip, &header, sizeof(header));
}

void tcp_rx_packet(uint32_t dst_ip, void *payload, int payload_tvb)
{
    tcb *i, *referenced_tcb = NULL;
    tcp_header *incoming = (tcp_header *)payload;

    tcp_swap_endian(incoming);

    /* Find the tcb that this packet was for. */
    list_for_each(i, &tcb_head, tcb_next)
        if (incoming->dest_port == i->src_port) {
            referenced_tcb = i;
            break;
        }

    if (referenced_tcb == NULL)
        return;

    /* We've received a packet for this tcb, stop any timeout
     * counters. */
    referenced_tcb->decrement_timeout = 0;
    referenced_tcb->timed_out = 0;
    referenced_tcb->timeout = 0;

    switch(referenced_tcb->state)
    {
    case SYN_SENT:
        /* At this point the client could refuse the connection [RST,
         * ACK] or accept the connection [SYN, ACK]. */
        if (incoming->syn && incoming->ack) {
            tcp_header response;

            referenced_tcb->cur_seq_n++;
            referenced_tcb->cur_ack_n = incoming->seq_n + 1;
            referenced_tcb->host_window_sz = incoming->window_sz;

            memset(&response, 0, sizeof(response));

            tcp_header_prepopulate(referenced_tcb, &response);

            response.ack = 1;

            tcp_tx(response, referenced_tcb->dst_ip);

            referenced_tcb->state = ESTABLISHED;
            return;
        }

        referenced_tcb->state = CLOSED;
        return;
    }
}

/* Perform a 3-way handshake and establish a TCP connection. */
tcb *tcp_connect(uint16_t port, uint32_t ip)
{
    tcp_header header;
    tcb *new_tcb = get_mem(sizeof(*new_tcb));

    memset(&header, 0, sizeof(header));
    memset(new_tcb, 0, sizeof(*new_tcb));

    new_tcb->cur_seq_n = 1024;
    new_tcb->state = SYN_SENT;
    new_tcb->timeout = TCP_TIMEOUT;
    new_tcb->decrement_timeout = 1;
    new_tcb->src_port = 65355;
    new_tcb->dst_port = port;
    new_tcb->last_msg = &header;
    new_tcb->dst_ip = ip;

    tcp_header_prepopulate(new_tcb, &header);

    header.syn = 1;

    list_add(&new_tcb->tcb_next, &tcb_head);

    tcp_tx(header, ip);

    wait_for_volatile_condition(new_tcb->state != SYN_SENT);

    if (new_tcb->state == ESTABLISHED)
        return new_tcb;

    list_del(&new_tcb->tcb_next);
    free_mem(new_tcb);
    return NULL;
}

void tcp_tick(void)
{
    tcb *cur;
    for_each_tcb(cur)
        if (cur->decrement_timeout)
            if (!cur->timeout--) {
                cur->timed_out = 1;
                cur->decrement_timeout = 0;
            }
}

static struct tick_work_q tcp_tick_work = {
    .tick_fn = tcp_tick
};

void tcp_init(void)
{
    tick_add_work_fn(&tcp_tick_work);
}
initcall(tcp_init);
