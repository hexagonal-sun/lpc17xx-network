#include "lpc17xx.h"
#include "ethernet.h"
#include "emac.h"
#include "memory.h"
#include "arp.h"
#include "irq.h"
#include "tick.h"
#include <math.h>

int main(void) {
    uint8_t mac_addr[6] = {0, 1, 2, 3, 4, 5};

    ethernet_init(mac_addr);

    /* Enable Ethernet Mac interrupts. */
    LPC_NVIC->ISER0 = (1 << 28);

    __irq_enable();

    while (1) {
        __asm__("wfi");
    }
}
