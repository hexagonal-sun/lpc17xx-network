#include "irq.h"
#include "lpc17xx.h"
#include "tick.h"
#include "process.h"
#include "init.h"

LIST(tick_work);
static int timer0_irq_wakeups = 0;

void irq_timer0(void)
{
    irq_flags_t flags = irq_disable();
    timer0_irq_wakeups++;

    /* Invoke all tick functions. */
    struct tick_work_q *i;

    list_for_each(i, &tick_work, tick_work_l) {
        i->tick_fn();
    }

    irq_enable(flags);

    /* Acknowledge the interrupt. */
    LPC_TIM0->IR = 0x1;

    /* Raise a PendSV for context switch. */
    LPC_SCB->ICSR |= ICSR_PENDSVSET_MASK;
}

void tick_add_work_fn(struct tick_work_q *new_work)
{
    irq_flags_t flags = irq_disable();
    list_add(&new_work->tick_work_l, &tick_work);
    irq_enable(flags);
}

void tick_init(void)
{
    /* CCLK is 100MHZ.  When we divide this by 4 (as PCLK_TIMER0 is
     * equal to 00 by default), we have a counter of 25MHZ. */
#define TICK_PERIOD (5e-3)
#define PCLOCK_FREQ (25e6)
#define TICK_FREQ (1 / TICK_PERIOD)
#define TIMER_COUNTER_INIT_VALUE PCLOCK_FREQ / TICK_FREQ

    LPC_TIM0->PR = 0x0;
    LPC_TIM0->MR0 = TIMER_COUNTER_INIT_VALUE;

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
