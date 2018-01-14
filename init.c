#include "init.h"
#include "lpc17xx.h"
#include "irq.h"
#include "memory.h"
#include <stdint.h>


extern uint8_t _etext, _sdata, _edata, _sbss, _ebss;
extern initcall_t _sinitcalls, _einitcalls;

static void call_initcalls()
{
    initcall_t *cur = &_sinitcalls;

    while (cur != &_einitcalls)
        (*cur++)();
}

static void init_clocking(void)
{
    /* Enable the main oscillator and set the correct frequency range
     * (12Mhz). */
    LPC_SC->SCS = (1 << 5);

    /* Wait for Main oscillator startup. */
    while (!(LPC_SC->SCS & (1 << 6))) {};

    /* Set sysclk to the main oscillator. */
    LPC_SC->CLKSRCSEL = 0x1;

    /* Output from the PLL will be 300 Mhz.  We want to divide this
     * down by 3 to produce the CCLK of 100 Mhz. */
    LPC_SC->CCLKCFG = 0x2;

    /* We want an output of 300Mhz from the PLL.  Assuming a
     * pre-divide of 2, M = (300 * 2) / (2 * 12).  Therefore, set: M =
     * 25 and N = 2. */
    LPC_SC->PLL0CFG = 24 | (1 << 16);

    /* Provide the PLL feed sequence. */
    LPC_SC->PLL0FEED = 0xAA;
    LPC_SC->PLL0FEED = 0x55;

    /* Enable the PLL. */
    LPC_SC->PLL0CON = 0x01;

    /* Provide the PLL feed sequence. */
    LPC_SC->PLL0FEED = 0xAA;
    LPC_SC->PLL0FEED = 0x55;

    /* Wait for PLL0 lock */
    while (!(LPC_SC->PLL0STAT & (1 << 26))) {};

    /* Enable and connect PLL0 */
    LPC_SC->PLL0CON = 0x3;

    /* Provide the PLL feed sequence. */
    LPC_SC->PLL0FEED = 0xAA;
    LPC_SC->PLL0FEED = 0x55;

    /* Wait until PLL0 enable and connect. */
    while (!(LPC_SC->PLL0STAT & ((1 << 25) | (1 << 24)))) {};

    /* CPU should now be operating at 100 Mhz. */
}

/* Called straight after reset.  Copy over the data, zero the BSS and
 * call main. */
void __attribute__((noreturn)) _start()
{
    uint8_t *src = &_etext;
    uint8_t *dst = &_sdata;

    /* Copy the data into RAM. */
    while (dst < &_edata)
        *dst++ = *src++;

    /* Zero the BSS. */
    for (dst = &_sbss; dst < &_ebss; dst++)
        *dst = 0;

    /* Bring the MCU up to 100 Mhz operation. */
    init_clocking();

    mem_setup();

    call_initcalls();

    __irq_enable();

    __asm__ volatile ("b .");
    __builtin_unreachable();
}
