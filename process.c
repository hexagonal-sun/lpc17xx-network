#include "process.h"
#include "list.h"
#include "irq.h"
#include "memory.h"
#include "init.h"
#include "lpc17xx.h"
#include <stdint.h>
#include <string.h>

static LIST(runqueue);
static LIST(waitqueue);
static LIST(deadqueue);

static process_t *current_tsk = NULL;
static process_t *idle_tsk;

#define STACK_SZ 0x400

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
    irq_flags_t flags = irq_disable();
    list_del(&proc->cur_sched_queue);

    list_add(&proc->cur_sched_queue, &runqueue);
    proc->state = RUNNING;
    irq_enable(flags);
}

/* Put the current process on the waitqueue and force a context switch
 * to another process.
 *
 * Should be called with interrupts disabled.
 */
void process_wait(void)
{
    __irq_disable();

    list_add(&current_tsk->cur_sched_queue, &waitqueue);
    current_tsk->state = WAITING;
    reschedule();
}

static void process_finish()
{
    __irq_disable();

    list_add(&current_tsk->cur_sched_queue, &deadqueue);
    current_tsk->state = FINISHED;
    reschedule();
}

static process_t *create_process(memaddr_t pc, memaddr_t r0)
{
    process_t *new_process = get_mem(sizeof(*new_process));
    hw_stack_ctx *new_hw_stack_ctx;
    sw_stack_ctx *new_sw_stack_ctx;

    /* We ask the heap for some new stack space. */
    new_process->stack_alloc = get_mem(STACK_SZ);

    /* Since on the cortex-m, the stack is descending, move the
     * current stack pointer to the end of the allocation.  Reserve
     * space for the hw_stack_ctx too, as this will be pop'd off by
     * the HW when the process is first executed. */
    new_hw_stack_ctx = (new_process->stack_alloc + STACK_SZ) -
        sizeof(hw_stack_ctx);

    /* Allocate space in the new stack for the SW stack context, as
     * this will be pop'd by the SW when the process is first
     * executed. */
    new_sw_stack_ctx = new_process->cur_stack =
        (void *)new_hw_stack_ctx - sizeof(sw_stack_ctx);

    memset(new_hw_stack_ctx, 0, sizeof(*new_hw_stack_ctx));
    memset(new_sw_stack_ctx, 0, sizeof(*new_sw_stack_ctx));

    new_hw_stack_ctx->pc = pc;
    new_hw_stack_ctx->lr = (memaddr_t)process_finish;
    new_hw_stack_ctx->r0 = r0;
    new_hw_stack_ctx->psr = 0x01000000;

    return new_process;
}

void process_spawn(memaddr_t pc, memaddr_t r0)
{
    process_t *newproc = create_process(pc, r0);
    irq_flags_t flags = irq_disable();

    list_add(&newproc->cur_sched_queue, &runqueue);
    newproc->state = RUNNING;
    irq_enable(flags);
}

static void __idle_task(void)
{
    process_t *dead_process;

    while (1) {
        irq_flags_t flags = irq_disable();
        list_pop(dead_process, &deadqueue, cur_sched_queue);

        if (dead_process) {
            free_mem(dead_process->stack_alloc);
            free_mem(dead_process);
        }

        irq_enable(flags);

        asm volatile("wfi");
    }
}

static void process_init(void)
{
    extern thread_t _sthreads, _ethreads;
    thread_t *cur = &_sthreads;
    uint32_t zero = 0;

    while (cur != &_ethreads)
        process_spawn((memaddr_t)*cur++, 0);

    idle_tsk = create_process((memaddr_t)&__idle_task, 0);

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
    irq_flags_t flags = irq_disable();

    if (current_tsk) {

        if (!current_stack)
            /* fatal error - an active task should always return a
             * stack. */
            asm volatile("b .");

        current_tsk->cur_stack = current_stack;

        /* Since we could be called for a process that has just been
         * put to sleep, ensure the process is in a RUNNING state
         * before adding back to the runqueue. */
        if (current_tsk->state == RUNNING)
            list_add_tail(&current_tsk->cur_sched_queue, &runqueue);
    }

    /* Pick the head of the runqueue. */
    list_pop(next, &runqueue, cur_sched_queue);

    if (!next)
        next = idle_tsk;

    /* Set as current task. */
    current_tsk = next;

    irq_enable(flags);

    return next->cur_stack;
}
