#include <stdint.h>

void main();

extern uint8_t _etext, _sdata, _edata, _sbss, _ebss;

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

    /* Call main. */
    main();

    __asm__ volatile ("b .");
    __builtin_unreachable();
}
