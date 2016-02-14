#include "tcp.h"
#include "byteswap.h"
#include "ipv4.h"
#include "ethernet.h"
#include "memory.h"
#include "tick.h"
#include "init.h"
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

void tcp_rx_packet(uint32_t dst_ip, void *payload, int payload_len)
{
    return;
}

/* Perform a 3-way handshake and establish a TCP connection. */
tcb *tcp_connect(uint16_t port, uint32_t ip)
{
    tcp_header header;
    tcp_pseudo pheader;
    tcb *new_tcb = get_mem(sizeof(*new_tcb));

    memset(&header, 0, sizeof(header));
    memset(&pheader, 0, sizeof(pheader));
    memset(new_tcb, 0, sizeof(*new_tcb));

    header.source_port = 65355;
    header.dest_port = port;
    header.seq_n = 1024;
    header.syn = 1;
    header.data_offset = 5;

    pheader.src = OUR_IP_ADDRESS;
    pheader.dst = ip;
    pheader.proto = IP_PROTO_TCP;
    pheader.length = sizeof(header);

    tcp_swap_endian(&header);
    tcp_swap_pseudo_endian(&pheader);

    tcp_compute_checksum(&header, &pheader);

    new_tcb->cur_seq_n = 1024;
    new_tcb->state = SYN_SENT;
    new_tcb->timeout = TCP_TIMEOUT;
    new_tcb->decrement_timeout = 1;
    new_tcb->src_port = 65355;
    new_tcb->dst_port = port;
    new_tcb->last_msg = &header;

    list_add(&new_tcb->tcb_next, &tcb_head);

    ip4_xmit_packet(IP_PROTO_TCP, ip, &header, sizeof(header));

    while (!new_tcb->timed_out)
        __asm__ volatile ("wfi");

    if (new_tcb->timed_out)
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
