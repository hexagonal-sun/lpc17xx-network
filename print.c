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

static void print_digits(int64_t num, uint8_t radix,
                         char hex_base)
{
    uint8_t digit;
    char char_to_print;

    /* Base case. */
    if (!num)
        return;

    digit = num % radix;

    char_to_print = digit > 9 ? hex_base + (digit - 10) : '0' + digit;
    print_digits(num / radix, radix, hex_base);
    _putc(char_to_print);
}

static void print_number(int64_t num, uint8_t radix,
                         char hex_base, int is_unsigned)
{
    if (num == 0) {
        _putc('0');
        return;
    }

    if (!is_unsigned)
        /* Handle negative numbers. */
        if (num < 0) {
            _putc('-');
            num = -num;
        }

    print_digits(num, radix, hex_base);
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
                print_number(tmp, 10, 'a', 0);
                break;
            }
            case 'x':
            {
                int tmp = va_arg(args, int);
                print_number(tmp, 16, 'a', 1);
                break;
            }
            case 'X':
            {
                int tmp = va_arg(args, int);
                print_number(tmp, 16, 'A', 1);
                break;
            }
            case 's':
            {
                const char *str = va_arg(args, const char *);
                _puts(str);
                break;
            }
            case 'p':
            {
                const uint32_t p = va_arg(args, uint32_t);
                _puts("0x");
                print_number(p, 16, 'A', 1);
                break;
            }
            }
        } else
            _putc(*fmt);

        fmt++;
    }
    va_end(args);
}
