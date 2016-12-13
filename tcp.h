#pragma once
#include "list.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t seq_n;
    uint32_t ack_n;
    uint8_t __reserved_0 : 4;
    uint8_t data_offset : 4;
    uint8_t fin : 1;
    uint8_t syn : 1;
    uint8_t rst : 1;
    uint8_t psh : 1;
    uint8_t ack : 1;
    uint8_t urg : 1;
    uint8_t ece : 1;
    uint8_t cwr : 1;
    uint16_t window_sz;
    uint16_t checksum;
    uint16_t urg_ptr;
} __attribute__((packed)) tcp_header;

enum tcp_state {
    CLOSED,
    SYN_SENT,
    ESTABLISHED,
    CLOSE_WAIT,
    LAST_ACK
};

typedef struct
{
    uint32_t cur_seq_n;
    uint32_t cur_ack_n;
    uint32_t unacked_byte_count;
    enum tcp_state state;
    uint16_t timeout;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t host_window_sz;
    uint32_t dst_ip;
    uint8_t  decrement_timeout : 1;
    uint8_t  timed_out : 1;
    tcp_header *last_msg;
    list tcb_next;
} tcb;

void tcp_rx_packet(uint32_t dst_ip, void *payload, int payload_len);

/* Perform a 3-way handshake and establish a TCP connection. */
tcb *tcp_connect(uint16_t port, uint32_t ip);

/* Send data down an already-established TCP connection. */
void tcp_tx_data(tcb *connection, void *data, size_t len);

#define for_each_tcb(pos)                       \
    list_for_each((pos), &tcb_head, tcb_next)
