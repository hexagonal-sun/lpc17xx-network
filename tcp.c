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
#define TCP_BUF_SZ 127

typedef struct
{
    uint32_t src;
    uint32_t dst;
    uint8_t __reserved_0;
    uint8_t proto;
    uint16_t length;
} tcp_pseudo;


static WAITQUEUE(tcp_waitq);
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

static void tcp_compute_checksum(tcp_header *header, tcp_pseudo *pheader,
                                 void *payload_data, size_t payload_len)
{
    void *buf;
    uint16_t *x;
    size_t buf_sz = sizeof(*header) + sizeof(*pheader) + payload_len;
    uint32_t sum = 0;
    int i;

    if (buf_sz % 2)
        buf_sz += 1;

    buf = get_mem(buf_sz);
    memset(buf, 0, buf_sz);

    memcpy(buf, pheader, sizeof(*pheader));
    memcpy(buf + sizeof(*pheader), header, sizeof(*header));
    memcpy(buf + sizeof(*pheader) + sizeof(*header), payload_data, payload_len);

    x = buf;

    for (i = 0; i < buf_sz / 2; i++)
        sum += x[i];

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    header->checksum = ~sum;

    free_mem(buf);
}

static void tcp_header_prepopulate(tcb *t, tcp_header *header)
{
    header->source_port = t->src_port;
    header->dest_port = t->dst_port;
    header->seq_n = t->cur_seq_n;
    header->ack_n = t->cur_ack_n;
    header->data_offset = 5;

    header->window_sz = circular_buf_cur_capacity(&t->rx_buf);
}

static void tcp_tx(tcp_header header, uint32_t dest_ip,
                   void *payload, size_t payload_len)
{
    tcp_pseudo pheader;
    void *buf = &header;

    memset(&pheader, 0, sizeof(pheader));

    pheader.src = OUR_IP_ADDRESS;
    pheader.dst = dest_ip;
    pheader.proto = IP_PROTO_TCP;
    pheader.length = sizeof(header) + payload_len;

    tcp_swap_endian(&header);
    tcp_swap_pseudo_endian(&pheader);

    tcp_compute_checksum(&header, &pheader, payload, payload_len);

    buf = get_mem(sizeof(header) + payload_len);

    memcpy(buf, &header, sizeof(header));
    memcpy(buf + sizeof(header), payload, payload_len);

    ip4_xmit_packet(IP_PROTO_TCP, dest_ip, buf,
                    sizeof(header) + payload_len);
}

void tcp_rx_packet(uint32_t dst_ip, void *payload, int payload_len)
{
    tcb *i, *referenced_tcb = NULL;
    tcp_header *incoming = (tcp_header *)payload;
    size_t tcp_header_sz = incoming->data_offset * 4;
    size_t data_len = payload_len - tcp_header_sz;
    int send_ack = 0;

    tcp_swap_endian(incoming);

    /* Find the TCB that this packet was for. */
    list_for_each(i, &tcb_head, tcb_next)
        if (incoming->dest_port == i->src_port &&
            incoming->source_port == i->dst_port &&
            dst_ip == i->dst_ip) {
            referenced_tcb = i;
            break;
        }

    /* If we didn't find a TCB, see if there are any TCBs that are in
     * the listening state that can accept this packet as it may be
     * the start of a new connection. */
    if (!referenced_tcb) {
        list_for_each(i, &tcb_head, tcb_next)
            if (i->state == LISTEN &&
                incoming->dest_port == i->src_port) {
                referenced_tcb = i;
                break;
            }
    }

    if (referenced_tcb == NULL) {
        tcp_header response;

        /* We couldn't find a TCB for the referenced connection.
         * which is equivalent to the connection being in a CLOSED
         * state. */
        if (incoming->rst)
            return;

        memset(&response, 0, sizeof(response));

        response.rst = 1;
        response.data_offset = 5;
        response.dest_port = incoming->source_port;
        response.source_port = incoming->dest_port;

        if (incoming->syn)
            data_len += 1;

        if (incoming->ack)
            response.seq_n = incoming->ack_n;
        else {
            response.ack_n = incoming->seq_n + data_len;
            response.ack = 1;
        }

        tcp_tx(response, dst_ip, NULL, 0);
        return;
    }

    /* We've received a packet for this tcb, stop any timeout
     * counters. */
    referenced_tcb->decrement_timeout = 0;
    referenced_tcb->timed_out = 0;
    referenced_tcb->timeout = 0;

    if (referenced_tcb->state == CLOSE_WAIT) {
        __irq_disable();
        asm("b .");
    }

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

            tcp_tx(response, referenced_tcb->dst_ip, NULL, 0);

            referenced_tcb->state = ESTABLISHED;
            break;
        }

        referenced_tcb->state = CLOSED;
        break;

    case LISTEN:
        if (incoming->syn) {
            referenced_tcb->dst_ip = dst_ip;
            referenced_tcb->cur_ack_n = incoming->seq_n + 1;
            referenced_tcb->dst_port = incoming->source_port;
            referenced_tcb->state = SYN_RECEIVED;
        }
        break;

    case SYN_RECEIVED:
        if (incoming->ack) {
            referenced_tcb->cur_seq_n = incoming->ack_n;
            referenced_tcb->state = ESTABLISHED;
        }
        break;
    case ESTABLISHED:
    {
        int no_acked_bytes = 0;

        if (incoming->ack) {
            no_acked_bytes = incoming->ack_n -
                referenced_tcb->cur_seq_n;

            if (no_acked_bytes > 0)
                referenced_tcb->unacked_byte_count -= no_acked_bytes;

            referenced_tcb->cur_seq_n = incoming->ack_n;
        }

        if (incoming->fin) {
            referenced_tcb->state = CLOSE_WAIT;
            referenced_tcb->cur_ack_n++;
            send_ack = 1;
        }

        if (data_len) {
            uint8_t *data_buf = payload + sizeof(tcp_header);
            circular_buf_push(&referenced_tcb->rx_buf, data_buf, data_len);

            referenced_tcb->cur_ack_n += data_len;
            send_ack = 1;
        }

        if (!data_len && !no_acked_bytes)
            send_ack = 1;
        break;
    }
    case CLOSE_WAIT:
    {
        if (incoming->ack &&
            incoming->ack_n == referenced_tcb->cur_ack_n + 1) {
            circular_buf_free(&referenced_tcb->rx_buf);
            list_del(&referenced_tcb->tcb_next);
            free_mem(referenced_tcb);

            return;
        }
    }
    }

    if (send_ack)
    {
        tcp_header resp;

        memset(&resp, 0, sizeof(resp));

        tcp_header_prepopulate(referenced_tcb, &resp);

        resp.ack = 1;

        tcp_tx(resp, dst_ip, NULL, 0);
    }

    waitqueue_wakeup(&tcp_waitq);
}

/* Perform a 3-way handshake and establish a TCP connection. */
tcb *tcp_connect(uint16_t port, uint32_t ip)
{
    tcp_header header;
    tcb *new_tcb = get_mem(sizeof(*new_tcb));

    memset(&header, 0, sizeof(header));
    memset(new_tcb, 0, sizeof(*new_tcb));

    circular_buf_init(&(new_tcb->rx_buf), TCP_BUF_SZ);

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

    tcp_tx(header, ip, NULL, 0);

    wait_for_volatile_condition(new_tcb->state != SYN_SENT,
                                tcp_waitq);;

    if (new_tcb->state == ESTABLISHED)
        return new_tcb;

    list_del(&new_tcb->tcb_next);
    free_mem(new_tcb);
    return NULL;
}

tcb *tcp_listen(uint16_t port)
{
    tcp_header header, resp;
    tcb *new_tcb = get_mem(sizeof(*new_tcb));

    memset(&header, 0, sizeof(header));
    memset(new_tcb, 0, sizeof(*new_tcb));

    circular_buf_init(&(new_tcb->rx_buf), TCP_BUF_SZ);

    new_tcb->cur_seq_n = 1024;
    new_tcb->state = LISTEN;
    new_tcb->src_port = port;
    new_tcb->dst_port = 0;
    new_tcb->dst_ip = 0;

    list_add(&new_tcb->tcb_next, &tcb_head);

    wait_for_volatile_condition(new_tcb->state == SYN_RECEIVED,
                                tcp_waitq);

    memset(&resp, 0, sizeof(resp));

    tcp_header_prepopulate(new_tcb, &resp);

    resp.syn = 1;
    resp.ack = 1;

    tcp_tx(resp, new_tcb->dst_ip, NULL, 0);

    wait_for_volatile_condition(new_tcb->state == ESTABLISHED,
                                tcp_waitq);

    return new_tcb;
}

void tcp_tx_data(tcb *connection, void *data, size_t len)
{
    tcp_header header;

    memset(&header, 0, sizeof(header));

    tcp_header_prepopulate(connection, &header);

    header.ack = 1;

    connection->unacked_byte_count += len;

    tcp_tx(header, connection->dst_ip, data, len);

    wait_for_volatile_condition(!connection->unacked_byte_count,
                                tcp_waitq);
}

int tcp_rx_data(tcb *connection, void *dst_buf, size_t len)
{
    while (len) {
        size_t no_bytes_to_copy, bytes_in_buf;

        wait_for_volatile_condition(
            (circular_buf_cur_usage(&connection->rx_buf) != 0)
            || connection->state == CLOSE_WAIT,
            tcp_waitq);

        if (connection->state == CLOSE_WAIT)
            return -1;

        bytes_in_buf = circular_buf_cur_usage(&connection->rx_buf);
        no_bytes_to_copy = bytes_in_buf > len ? len : bytes_in_buf;

        circular_buf_pop(&connection->rx_buf, dst_buf, no_bytes_to_copy);

        len -= no_bytes_to_copy;
        dst_buf += no_bytes_to_copy;
    }

    return 0;
}

void tcp_close(tcb *connection)
{
    if (connection->state == CLOSE_WAIT) {
        tcp_header our_fin;

        memset(&our_fin, 0, sizeof(our_fin));

        tcp_header_prepopulate(connection, &our_fin);

        our_fin.fin = 1;
        our_fin.ack = 1;

        tcp_tx(our_fin, connection->dst_ip, NULL, 0);

        connection->state = LAST_ACK;
    }

    /* Todo: close active connection. */
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
