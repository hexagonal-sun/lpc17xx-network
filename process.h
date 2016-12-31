#pragma once
#include <stdint.h>

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

typedef void (*thread_t)(void);

#define thread(fn)                                     \
    static volatile thread_t __thread_##fn             \
    __attribute__((__section__(".threads"))) = fn;
