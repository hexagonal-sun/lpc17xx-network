#include "init.h"
#include "lpc17xx.h"
#include "irq.h"
#include <stdint.h>
#include <stddef.h>

extern uint8_t _sbss, _ebss;
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
void __attribute__((noreturn)) _start(void *virt_addr,
                                      void *phys_addr,
                                      void *pgtb_addr)
{
    uint8_t *i;

    /* Zero the BSS. */
    for (i = &_sbss; i < &_ebss; i++)
        *i = 0;

    memory_init();

    /* call_initcalls(); */

    /* __irq_enable(); */

    main();

    __asm__ volatile ("b .");
    __builtin_unreachable();
}
