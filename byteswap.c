#include "byteswap.h"

void swap_endian16(uint16_t *val)
{
    *val = (*val<<8) | (*val>>8);
}

void swap_endian32(uint32_t *val)
{
    *val = (*val<<24) | ((*val<<8) & 0x00ff0000) |
        ((*val>>8) & 0x0000ff00) | (*val>>24);
}

