#include "udp.h"
#include "tcp.h"
#include "process.h"
#include <string.h>

    char *str = "Hello, UDP!\n";
void main(void) {
    void *tcp_connection;

    tcp_connection = tcp_connect(5757, 0xc0a80476);

    tcp_tx_data(tcp_connection, str, strlen(str));

    while (1) {
        __asm__("wfi");
    }
}
thread(main);
