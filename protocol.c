#include "irq.h"
#include "wait.h"
#include "memory.h"
#include "protocol.h"

static LIST(protocol_head);
static LIST(pkt_rx_q);
static WAITQUEUE(rx_waitq);

struct packet_t *packet_create(void *frame, size_t frame_len)
{
    struct packet_t *ret = get_mem(sizeof(*ret));

    ret->data = ret->cur_data = frame;
    ret->data_length = ret->cur_data_length = frame_len;

    return ret;
}

void packet_destroy(struct packet_t *pkt)
{
    free_mem(pkt->data);
    free_mem(pkt);
}

/* Inject a packet_t into the networking stack.  The destination
 * parameter should state the first protocol layer to be used for the
 * packet. */
void packet_inject(struct packet_t *pkt, enum protocol_type type)
{
    irq_flags_t flags = irq_disable();
    pkt->handler = type;
    list_add(&pkt->cur_q, &pkt_rx_q);
    waitqueue_wakeup(&rx_waitq);
    irq_enable(flags);
}

void protocol_register(struct protocol_t *protocol)
{
    list_add(&protocol->next_protocol, &protocol_head);
}

static struct protocol_t *resolve_pkt_protocol(struct packet_t *pkt)
{
    struct protocol_t *proto;

    for_each_protocol(proto) {
        if (proto->type == pkt->handler)
            return proto;
    }

    return NULL;
}

static void rx_task(void)
{
    while (1) {
        irq_flags_t flags;
        struct packet_t *pkt;

        wait_for_volatile_condition(!list_empty(&pkt_rx_q), rx_waitq);

        flags = irq_disable();
        list_pop(pkt, &pkt_rx_q, cur_q);
        irq_enable(flags);

        while (pkt) {
            struct protocol_t *proto = resolve_pkt_protocol(pkt);

            if (!proto)
                /* We couldn't find a protocol handler for this packet.
                 * Drop it. */
                pkt->handler = DROP;
            else
                proto->rx_pkt(pkt);

            if (pkt->handler == DROP) {
                packet_destroy(pkt);
                pkt = NULL;
            }
        }
    }
}
thread(rx_task)
