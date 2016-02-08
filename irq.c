#include "irq.h"

inline void __irq_enable(void)
{
    __asm__ volatile("cpsie i");
}

inline void __irq_disable(void)
{
    __asm__ volatile("cpsid i");
}
