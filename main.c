#include "lpc17xx.h"
#include "ethernet.h"
#include "emac.h"
#include "memory.h"
#include "arp.h"
#include "irq.h"
#include "tick.h"
#include <math.h>

int main(void) {

    while (1) {
        __asm__("wfi");
    }
}
