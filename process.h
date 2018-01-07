#pragma once
#include <stdint.h>
#include "list.h"

typedef uint32_t memaddr_t;

typedef struct {
    memaddr_t r0;
    memaddr_t r1;
    memaddr_t r2;
    memaddr_t r3;
    memaddr_t r12;
    memaddr_t lr;
    memaddr_t pc;
    memaddr_t psr;
} hw_stack_ctx;

typedef struct {
    memaddr_t r4;
    memaddr_t r5;
    memaddr_t r6;
    memaddr_t r7;
    memaddr_t r8;
    memaddr_t r9;
    memaddr_t r10;
    memaddr_t r11;
} sw_stack_ctx;

typedef struct
{
    void *stack;
    list cur_sched_queue;
    enum {
        RUNNING,
        WAITING,
        FINISHED
    } state;
} process_t;

process_t *process_get_cur_task(void);
void process_wait(void);
void process_wakeup(process_t *proc);

typedef void (*thread_t)(void);

#define thread(fn)                                     \
    static volatile thread_t __thread_##fn             \
    __attribute__((__section__(".threads"))) = fn;
