#include "memory.h"
#include <math.h>

#define BLOCK_SZ 128

uint8_t *freelist;
int no_blocks;
void *pm_base;

static inline void *idx_2_addr(int idx)
{
    return (void *)(pm_base + (idx * BLOCK_SZ));
}

static inline int addr_2_idx(void *addr)
{
    return ((addr - pm_base) / BLOCK_SZ);
}

/* Length is in Kilobytes. */
void memory_init(void *base, uint16_t len)
{
    int i, blocks_used_by_freelist;
    freelist = base;

    /* Determine how many BLOCK_SZ byte blocks there are in len. */
    no_blocks = len << 3;        /* 8 = 1024 / BLOCK_SZ */

    /* We will use the first block for the MM structure. */
    no_blocks--;

    /* Set all blocks to free. */
    for (i = 0; i < no_blocks; i++)
        freelist[i] = 0;

    /* Find out how many blocks we need for the free list, and set
     * them as in use. */
    blocks_used_by_freelist = ceil((float)no_blocks / (float)BLOCK_SZ);
    for (i = 0; i < blocks_used_by_freelist; i++)
        freelist[i] = 0x1;

    pm_base = base;
}


void *get_mem(int len)
{
    int no_blocks_requested = (len / BLOCK_SZ) + 1,
        i, j;

    for (i = 0; i < no_blocks - no_blocks_requested; i++)
    {
        int all_blocks_free = 1;
        for (j = 0; j < no_blocks_requested; j++)
            if (freelist[i + j]) {
                all_blocks_free = 0;
                break;
            }
        if (all_blocks_free)
            break;
    }

    /* Not enough space for allocation. */
    if (i == (no_blocks - no_blocks_requested) - 1)
        return NULL;

    for (j = 0; j < no_blocks_requested; j++)
        freelist[i + j] = no_blocks_requested;

    return idx_2_addr(i);
}

void free_mem(void *addr)
{
    int idx = addr_2_idx(addr),
        no_blocks = freelist[idx],
        i;

    for (i = 0; i < no_blocks; i++)
        freelist[idx + i] = 0x0;
}
