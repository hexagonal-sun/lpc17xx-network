#include "process.h"
#include "list.h"
#include "irq.h"
#include "memory.h"
#include "init.h"
#include "lpc17xx.h"
#include <stdint.h>
#include <string.h>

LIST(runqueue);
LIST(waitqueue);

static process_t *current_tsk = NULL;
static process_t *idle_tsk;

static void reschedule()
{
    __irq_enable();

    /* Raise a PendSV for context switch. */
    LPC_SCB->ICSR |= ICSR_PENDSVSET_MASK;

    asm volatile("wfe");
}

process_t *process_get_cur_task(void)
{
    return current_tsk;
}

/* Move a given process from the waitqueue to the runqueue.
 */
void process_wakeup(process_t *proc)
{
    __irq_disable();
    list_del(&proc->cur_sched_queue);

    list_add(&proc->cur_sched_queue, &runqueue);
    proc->state = RUNNING;
    __irq_enable();
}

/* Put the current process on the waitqueue and force a context switch
 * to another process.
 *
 * Should be called with interrupts disabled.
 */
void process_wait(void)
{
    __irq_disable();
    list_del(&current_tsk->cur_sched_queue);

    list_add(&current_tsk->cur_sched_queue, &waitqueue);
    current_tsk->state = WAITING;
    reschedule();
}

static process_t *create_process(memaddr_t pc)
{
    static void *next_stack = (void *)0x10007C00;
    process_t *new_process = get_mem(sizeof(*new_process));
    hw_stack_ctx *new_hw_stack_ctx;
    sw_stack_ctx *new_sw_stack_ctx;

    new_hw_stack_ctx = next_stack - sizeof(hw_stack_ctx);;
    new_sw_stack_ctx = new_process->stack =
        (void *)new_hw_stack_ctx - sizeof(sw_stack_ctx);

    memset(new_hw_stack_ctx, 0, sizeof(*new_hw_stack_ctx));
    memset(new_sw_stack_ctx, 0, sizeof(*new_sw_stack_ctx));

    new_hw_stack_ctx->pc = pc;
    new_hw_stack_ctx->psr = 0x01000000;

    next_stack -= 0x400;    /* 1k per stack. */

    return new_process;
}

static void __idle_task(void)
{
    while (1)
        asm volatile("wfi");
}

static void process_init(void)
{
    extern thread_t _sthreads, _ethreads;
    thread_t *cur = &_sthreads;
    uint32_t zero = 0;

    while (cur != &_ethreads)
    {
        process_t *new_process = create_process((memaddr_t)*cur++);
        list_add(&new_process->cur_sched_queue, &runqueue);
        new_process->state = RUNNING;
    }

    idle_tsk = create_process((memaddr_t)&__idle_task);

    /* To kick off, we want the PSP to be NULL, so that irq_pendsv
     * doesn't attempt to stack values of an empty task. */
    asm volatile("msr psp, %0" : : "r"(zero));
}
initcall(process_init);

void *pick_new_task(void *current_stack)
{
    /* Select the next task to be executed in a simple round-robin
     * fashion.
     *
     * Should be called with interrupts disabled.*/
    process_t *next;

    if (current_tsk) {

        if (!current_stack)
            /* fatal error - an active task should always return a
             * stack. */
            asm volatile("b .");

        current_tsk->stack = current_stack;

        /* Since we could be called for a process that has just been
         * put to sleep, ensure the process is in a RUNNING state
         * before adding back to the runqueue. */
        if (current_tsk->state == RUNNING)
            list_add_tail(&current_tsk->cur_sched_queue, &runqueue);
    }

   /* Check for no cur_sched_queue. */
    if (list_empty(&runqueue)) {
        current_tsk = idle_tsk;
        return idle_tsk->stack;
    }

    /* Pick the head of the runqueue. */
    next = list_entry(runqueue.next, process_t, cur_sched_queue);

    /* Remove from the runqueue. */
    list_del(&next->cur_sched_queue);

    /* Set as current task. */
    current_tsk = next;

    return next->stack;
}
