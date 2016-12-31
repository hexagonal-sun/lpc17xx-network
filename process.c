#include "process.h"
#include "list.h"
#include "memory.h"
#include "init.h"
#include "lpc17xx.h"
#include <stdint.h>
#include <string.h>

typedef struct
{
    void *stack;
    list tasks;
} process_t;

LIST(runqueue);

static process_t *current_tsk = NULL;

static void process_init(void)
{
    extern thread_t _sthreads, _ethreads;
    static void *next_stack = (void *)0x10007C00;
    thread_t *cur = &_sthreads;
    uint32_t zero = 0;

    while (cur != &_ethreads)
    {
        process_t *new_process = get_mem(sizeof(*new_process));
        hw_stack_ctx *new_hw_stack_ctx;
        sw_stack_ctx *new_sw_stack_ctx;

        new_hw_stack_ctx = next_stack - sizeof(hw_stack_ctx);;
        new_sw_stack_ctx = new_process->stack =
            (void *)new_hw_stack_ctx - sizeof(sw_stack_ctx);

        memset(new_hw_stack_ctx, 0, sizeof(*new_hw_stack_ctx));
        memset(new_sw_stack_ctx, 0, sizeof(*new_sw_stack_ctx));

        new_hw_stack_ctx->pc = (uint32_t)(*cur++);
        new_hw_stack_ctx->psr = 0x01000000;

        list_add(&new_process->tasks, &runqueue);
        next_stack -= 0x400;    /* 1k per stack. */
    }

    /* To kick off, we want the PSP to be NULL, so that irq_pendsv
     * doesn't attempt to stack values of an empty task. */
    asm volatile("msr psp, %0" : : "r"(zero));
}
initcall(process_init);

void *pick_new_task(void *current_stack)
{
    /* Select the next task to be executed in a simple round-robin
     * fashion. */
    process_t *next;

    if (current_tsk) {

        if (!current_stack)
            /* fatal error - an active task should always return a
             * stack. */
            asm volatile("b .");

        current_tsk->stack = current_stack;

        list_add_tail(&current_tsk->tasks, &runqueue);
    }

   /* Check for no tasks. */
    if (list_empty(&runqueue)) {
        LPC_SCB->SCR |= SCR_SLEEPONEXIT_MASK;
        return NULL;
    }

    /* Pick the head of the runqueue. */
    next = list_entry(runqueue.next, process_t, tasks);

    /* Remove from the runqueue. */
    list_del(&next->tasks);

    /* Set as current task. */
    current_tsk = next;

    return next->stack;
}
