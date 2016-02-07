#pragma once
#pragma pack(push, 1)
#include <stdint.h>

#define ETHER_ADDR_LEN 6
#define OUR_IP_ADDRESS 0xC0A80002

/* Ethernet protocol. */
typedef struct
{
    uint8_t ether_dhost[ETHER_ADDR_LEN];
    uint8_t ether_shost[ETHER_ADDR_LEN];
    uint16_t ether_type;
} ethernet_header;

#define	ETHERTYPE_IP		0x0800
#define ETHERTYPE_ARP		0x0806

/* Construct an ethernet header and xmit the payload. */
void ether_xmit_payload(uint8_t dhost[ETHER_ADDR_LEN], uint16_t ether_type,
                        void *payload, int len);

void ether_rx_frame(void *frame, int frame_len);

/*
 * Compare two MAC addresses for equality.
 *
 * @returns 0 if the addresses do not match, 1 otherwise.
 */
int ethernet_mac_equal(uint8_t *a, uint8_t *b);

/*
 * Copy a MAC address from `src' into `dst'.  `dst' must already be
 * allocated.
 */
void ethernet_mac_copy(uint8_t *dst, uint8_t *src);
