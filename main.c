#include "lpc17xx.h"
#include "ethernet.h"
#include "memory.h"
#include "arp.h"
#include <math.h>

void irq_timer0()
{
    resolve_address(0xC0A8000C);

    /* Acknowledge the interrupt. */
    LPC_TIM0->IR = 0x1;
}

int main(void) {
    uint8_t mac_addr[6] = {0, 1, 2, 3, 4, 5};
    memory_init((void *)0x2007C000, 16);

    ethernet_init(mac_addr);

    LPC_TIM0->PR = 0x0;
    LPC_TIM0->MR0 = 0xFFFFFF;

    /* Clear the timer and prescale counter registers. */
    LPC_TIM0->TC = 0x0;
    LPC_TIM0->PC = 0x0;

    /* Reset and interrupt on TC==MR0. */
    LPC_TIM0->MCR = 0x3;

    /* Enable Timer 0 interrupts. */
    LPC_NVIC->ISER0 = (1 << 1);

    /* Enable Ethernet Mac interrupts. */
    LPC_NVIC->ISER0 = (1 << 28);

    __asm__("cpsie i");

    LPC_TIM0->TCR = 0x1;

    while (1) {
        __asm__("wfi");
    }
}
