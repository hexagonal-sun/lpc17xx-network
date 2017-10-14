#include "udp.h"
#include "atomics.h"
#include "ipv4.h"
#include "byteswap.h"
#include "memory.h"
#include "list.h"
#include "error.h"
#include "wait.h"
#include <string.h>

typedef struct
{
    uint8_t *dst_buf;
    uint16_t dst_buf_sz;
    uint16_t dst_buf_ptr;
    uint16_t port;
    list rx_requests;
}udp_listener;

static WAITQUEUE(udp_waitq);
static LIST(udp_rx_requests);
mutex_t udp_rx_requests_lock;

static void udp_swap_endian(udp_header *header)
{
    swap_endian16(&header->dst_port);
    swap_endian16(&header->length);
}

int udp_rx(uint16_t port, void *dst_buf, uint16_t dst_buf_sz)
{
    udp_listener *i;
    udp_listener newListener;

    get_lock(&udp_rx_requests_lock);

    list_for_each(i, &udp_rx_requests, rx_requests)
        if (i->port == port)
            return -EINUSE;

    newListener.port = port;
    newListener.dst_buf = dst_buf;
    newListener.dst_buf_sz = dst_buf_sz;
    newListener.dst_buf_ptr = 0;

    list_add(&newListener.rx_requests, &udp_rx_requests);

    release_lock(&udp_rx_requests_lock);

    wait_for_volatile_condition(newListener.dst_buf_ptr ==
                                newListener.dst_buf_sz,
                                udp_waitq);

    get_lock(&udp_rx_requests_lock);
    list_del(&newListener.rx_requests);
    release_lock(&udp_rx_requests_lock);

    return newListener.dst_buf_ptr;
}

void udp_rx_packet(uint32_t dst_ip, void *payload, int payload_len)
{
    udp_listener *i;
    udp_header *header = (udp_header *)payload;
    uint8_t *udp_payload = (payload + sizeof(udp_header));

    udp_swap_endian(header);

    size_t udp_payload_sz = header->length - sizeof(udp_header);

    if (try_lock(&udp_rx_requests_lock)) {
        list_for_each(i, &udp_rx_requests, rx_requests) {
            if (i->port == header->dst_port) {
                int user_buf_sz = i->dst_buf_sz - i->dst_buf_ptr,
                    no_bytes_to_copy = user_buf_sz;


                if (udp_payload_sz < no_bytes_to_copy)
                    no_bytes_to_copy = udp_payload_sz;

                memcpy(i->dst_buf + i->dst_buf_ptr, udp_payload, no_bytes_to_copy);

                i->dst_buf_ptr += no_bytes_to_copy;
                waitqueue_wakeup(&udp_waitq);
            }
        }
        release_lock(&udp_rx_requests_lock);
    }
}

void udp_xmit_packet(uint16_t dst_port, uint32_t dst_ip, void *payload,
                     int payload_len)
{
    int packet_buf_len = sizeof(udp_header) + payload_len;
    void *packet_buf = get_mem(packet_buf_len);
    udp_header *header = (udp_header *)packet_buf;

    header->src_port = 0;
    header->dst_port = dst_port;
    header->length = packet_buf_len;
    header->checksum = 0;

    udp_swap_endian(header);

    memcpy(packet_buf + sizeof(*header), payload, payload_len);

    ip4_xmit_packet(IP_PROTO_UDP, dst_ip, packet_buf, packet_buf_len);

    free_mem(packet_buf);
}
