#include "udp.h"
#include "tcp.h"
#include "process.h"
#include <string.h>

volatile uint8_t *UART = (uint8_t *)0x48020000;

void _puts(const char *str)
{
    while (*str != '\0')
        *UART = *(str++);
}

const char *hw = "Hello, World!";

void main(void) {
    _puts(hw);
    __asm__ volatile ("b .");
}
thread(main);
