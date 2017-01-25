#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "memory.h"
#include "print.h"

volatile uint8_t *UART = (uint8_t *)0x48020000;
volatile uint32_t *UART_status = (uint32_t *)0x48020044;

static void _putc(const char c)
{
    if (c == '\n')
        _putc('\r');

    while ((*UART_status) & 0x1) {}

    *UART = c;
}

static void print_number(int64_t num, uint8_t radix,
                         char hex_base)
{
    struct dig
    {
        char c;
        struct dig *next;
    } *dig_head;

    if (num == 0) {
        _putc('0');
        return;
    }

    /* Handle negative numbers. */
    if (num < 0) {
        _putc('-');
        num = -num;
    }

    dig_head = NULL;

    while (num != 0) {
        int digit = num % radix;
        struct dig *new_dig = get_mem(sizeof(*new_dig));

        new_dig->c = digit > 9 ? hex_base + (digit - 10) : '0' + digit;
        new_dig->next = dig_head;
        dig_head = new_dig;

        num /= radix;
    }

    while (dig_head)
    {
        struct dig *old_dig = dig_head;
        _putc(dig_head->c);
        dig_head = dig_head->next;
        free_mem(old_dig);
    }
}

static void _puts(const char *str)
{
    do {
        _putc(*(str++));
    } while (*str != '\0');
}

void printl(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    while (*fmt != '\0') {
        if (*fmt == '%') {

            fmt++;

            switch (*fmt) {
            case 'd':
            {
                int tmp = va_arg(args, int);
                print_number(tmp, 10, 'a');
                break;
            }
            case 'x':
            {
                int tmp = va_arg(args, int);
                print_number(tmp, 16, 'a');
                break;
            }
            case 'X':
            {
                int tmp = va_arg(args, int);
                print_number(tmp, 16, 'A');
                break;
            }
            case 's':
            {
                const char *str = va_arg(args, const char *);
                _puts(str);
                break;
            }
            break;
            }
        } else
            _putc(*fmt);

        fmt++;
    }
    va_end(args);
}
