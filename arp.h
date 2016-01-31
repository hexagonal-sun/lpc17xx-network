#include <stdint.h>
#include "ethernet.h"

#pragma pack(push, 1)
/* ARP protocol. */
typedef struct
{
    uint16_t HTYPE;
    uint16_t PTYPE;
    uint8_t  HLEN;
    uint8_t  PLEN;
    uint16_t OPER;
    uint8_t  SHA[ETHER_ADDR_LEN];
    uint32_t SPA;
    uint8_t  THA[ETHER_ADDR_LEN];
    uint32_t TPA;
} arp_packet;

#define HTYPE_ETHERNET 1
#define OPER_REQUEST 1

uint8_t * resolve_address(uint32_t IpAddress);
