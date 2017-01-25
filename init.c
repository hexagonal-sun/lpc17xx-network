#include "init.h"
#include "lpc17xx.h"
#include "irq.h"
#include <stdint.h>


extern uint8_t _etext, _sdata, _edata, _sbss, _ebss;
extern initcall_t _sinitcalls, _einitcalls;

extern void main(void);

static void call_initcalls()
{
    initcall_t *cur = &_sinitcalls;

    while (cur != &_einitcalls)
        (*cur++)();
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

    memory_init();

    /* call_initcalls(); */

    /* __irq_enable(); */

    main();

    __asm__ volatile ("b .");
    __builtin_unreachable();
}
