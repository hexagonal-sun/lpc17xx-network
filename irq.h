#pragma once
#include <stdint.h>

typedef uint32_t irq_flags_t;

static inline void __irq_enable(void)
{
    __asm__ volatile("cpsie i" ::: "memory");
}

static inline void __irq_disable(void)
{
    __asm__ volatile("cpsid i" ::: "memory");
}

static inline irq_flags_t __read_psr(void)
{
    irq_flags_t state;

    __asm__ volatile("mrs %0, primask" : "=r"(state));

    return state;
}

static inline void __restore_psr(irq_flags_t state)
{
    __asm__ volatile("msr primask, %0" :: "r"(state));
}

static inline irq_flags_t irq_disable()
{
    irq_flags_t state = __read_psr();
    __irq_disable();
    return state;
}

static inline void irq_enable(irq_flags_t state)
{
    __restore_psr(state);
}
