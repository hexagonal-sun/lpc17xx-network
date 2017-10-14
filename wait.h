#pragma once

#include "irq.h"
#include "process.h"

#define wait_for_volatile_condition(condition, waitq)   \
    ({                                                  \
        for (;;) {                                      \
            asm volatile("" : : : "memory");            \
            __irq_disable();                            \
            if (condition) {                            \
                __irq_enable();                         \
                break;                                  \
            }                                           \
            __waitqueue_wait(&waitq);                   \
        }                                               \
    })

typedef list waitqueue_t;

#define WAITQUEUE(name) \
    waitqueue_t name = __LIST_INIT(name)

void __waitqueue_wait(waitqueue_t *waitq);
void waitqueue_wakeup(waitqueue_t *waitq);
