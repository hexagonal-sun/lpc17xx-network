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

/* Set by ether_init(). */
extern uint8_t ether_addr[ETHER_ADDR_LEN];

/* Initialise the ethernet layer includeing the MAC and PHY. */
void ethernet_init(uint8_t mac_address[6]);

/* Construct an ethernet header and xmit the payload. */
void ether_xmit_payload(uint8_t dhost[ETHER_ADDR_LEN], uint16_t ether_type,
                        void *payload, int len);
