#include "atomics.h"
#include "lpc17xx.h"
#include "tick.h"
#include "process.h"
#include "init.h"

LIST(tick_work);
static mutex_t tick_work_q_lock = 0;
static int timer0_irq_wakeups = 0;

void irq_timer0(void)
{
    timer0_irq_wakeups++;

    /* Invoke all tick functions. */
    if (try_lock(&tick_work_q_lock)) {
        struct tick_work_q *i;

        list_for_each(i, &tick_work, tick_work_l) {
            i->tick_fn();
        }

        release_lock(&tick_work_q_lock);
    }

    /* Acknowledge the interrupt. */
    LPC_TIM0->IR = 0x1;

    /* Raise a PendSV for context switch. */
    LPC_SCB->ICSR |= ICSR_PENDSVSET_MASK;
}

void tick_add_work_fn(struct tick_work_q *new_work)
{
    get_lock(&tick_work_q_lock);
    list_add(&new_work->tick_work_l, &tick_work);
    release_lock(&tick_work_q_lock);
}

void tick_init(void)
{
    LPC_TIM0->PR = 0x0;
    LPC_TIM0->MR0 = 0xFFFF;

    /* Clear the timer and prescale counter registers. */
    LPC_TIM0->TC = 0x0;
    LPC_TIM0->PC = 0x0;

    /* Reset and interrupt on TC==MR0. */
    LPC_TIM0->MCR = 0x3;

    /* Enable Timer 0 interrupts. */
    LPC_NVIC->ISER0 = (1 << 1);

    /* Start the timer. */
    LPC_TIM0->TCR = 0x1;
}
initcall(tick_init);
