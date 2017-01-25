#include "udp.h"
#include "tcp.h"
#include "process.h"
#include "print.h"
#include <string.h>

const char *hw = "Hello, World!";

void main(void) {
    for (int i = 0; i < 24; i++)
        printl("The value of i is 0x%d\n", i);
    __asm__ volatile ("b .");
}
thread(main);
