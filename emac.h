#pragma once

/* Set by ether_init(). */
extern uint8_t ether_addr[ETHER_ADDR_LEN];

/* Transmit a frame over the network. */
void emac_xmit_frame(ethernet_header *header,
                     void *payload, int payload_len);
