#pragma once

#include "irq.h"
#include "process.h"

#define wait_for_volatile_condition(condition, waitq)    \
    ({                                                   \
        for (;;) {                                       \
            irq_flags_t flags_##waitq;                   \
            asm volatile("" : : : "memory");             \
            flags_##waitq = irq_disable();               \
            if (condition) {                             \
                irq_enable(flags_##waitq);               \
                break;                                   \
            }                                            \
            __waitqueue_wait(&waitq);                    \
        }                                                \
    })

typedef list waitqueue_t;

#define WAITQUEUE(name) \
    waitqueue_t name = __LIST_INIT(name)

void __waitqueue_wait(waitqueue_t *waitq);
void waitqueue_wakeup(waitqueue_t *waitq);
