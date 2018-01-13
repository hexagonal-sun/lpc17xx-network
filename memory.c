#include "memory.h"
#include "init.h"
#include "irq.h"
#include <math.h>

#define BLOCK_SZ 128

static uint8_t *freelist;
static int no_blocks;
static void *pm_base;

static inline void *idx_2_addr(int idx)
{
    return (void *)(pm_base + (idx * BLOCK_SZ));
}

static inline int addr_2_idx(void *addr)
{
    return ((addr - pm_base) / BLOCK_SZ);
}

typedef struct
{
    void *base;
    uint16_t len;
} memory_region_t;

memory_region_t memory_regions [] =
{
    {
        .base = (void *)0x2007C000,
        .len = 16
    }
};

/* Length is in Kilobytes. */
static void memory_init()
{
    int i, blocks_used_by_freelist, len = memory_regions[0].len;
    void *base = memory_regions[0].base;

    freelist = base;

    /* Determine how many BLOCK_SZ byte blocks there are in len. */
    no_blocks = (len * 1024) / BLOCK_SZ;

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
early_initcall(memory_init);


void *get_mem(int len)
{
    int no_blocks_requested = (len / BLOCK_SZ) + 1,
        i, j;
    irq_flags_t flags;

    flags = irq_disable();
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
    irq_enable(flags);

    /* Not enough space for allocation. */
    if (i == (no_blocks - no_blocks_requested) - 1)
        return NULL;

    flags = irq_disable();
    for (j = 0; j < no_blocks_requested; j++)
        freelist[i + j] = no_blocks_requested;
    irq_enable(flags);

    return idx_2_addr(i);
}

void free_mem(void *addr)
{
    int idx = addr_2_idx(addr),
        no_blocks = freelist[idx],
        i;

    /* This isn't a critical section, as there should only ever be one
     * thread that owns a piece of memory (if that memory is free'd
     * twice that's not a synchronistaion issue).  Even if this
     * function is preempted half-way through writing to the freelist,
     * the get_mem function will only return memory that has already
     * been marked as free by this loop. */
    for (i = 0; i < no_blocks; i++)
        freelist[idx + i] = 0x0;
}
