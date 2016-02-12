#include "udp.h"
#include <string.h>

int main(void) {
    char *str = "Hello, UDP!\n";

    udp_xmit_packet(8080, 0xc0a80176, str, strlen(str));

    while (1) {
        __asm__("wfi");
    }
}
