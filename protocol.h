#pragma once
#include <stddef.h>
#include "list.h"


enum protocol_type {
    ETHERNET,
    IPV4,
    UDP,
    TCP,
    ARP,

    /* None indicates that this packet should be destroyed and
     * dropped. */
    DROP
};

struct ipv4_pkt_info {
    uint32_t src_ip;
    uint32_t dst_ip;
};

struct packet_t
{
    void *data;
    void *cur_data;
    size_t data_length;
    size_t cur_data_length;
    enum protocol_type handler;
    struct ipv4_pkt_info ip4_info;
    list cur_q;
};

typedef void (*rx_pkt_func_t)(struct packet_t *pkt);

struct protocol_t
{
    enum protocol_type type;
    rx_pkt_func_t rx_pkt;
    list next_protocol;
};

#define for_each_protocol(pos)        \
    list_for_each((pos), &protocol_head, next_protocol)

struct packet_t *packet_create(void *frame, size_t frame_len);
void packet_destroy(struct packet_t *pkt);
void packet_inject(struct packet_t *pkt, enum protocol_type type);
void protocol_register(struct protocol_t *protocol);
