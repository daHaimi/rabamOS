#include <stddef.h>
#include <stdint.h>
#include <kernel/uart.h>
#include <common/stdlib.h>
#include <stdarg.h>

/**
 * UART0 enabled by default on GPIO14 (UART0_TXD) and GPIO15 (UART0_RXD)
 * Default baudrate is 115200
 * Sets up the UART interface UART0 for reading and UART1 for Writing.
 * The default setting works with a data rate of 115200 Baud
 */

/**
 * Read UART status flags from register
 * @return uart_flags_t struct of the UART status flags
 */
uart_flags_t read_flags(void) {
    uart_flags_t flags;
    flags.as_int = mmio_read(UART0_FR);
    return flags;
}

/**
 * Prints a character to UART
 * @param c The character to send
 */
void uart_putc(unsigned char c) {
    uart_flags_t flags;
    do {
        flags = read_flags();
    } while ( flags.transmit_queue_full );
    mmio_write(UART0_DR, c);
}

/**
 * Print a string to UART
 * @param str The string to send
 */
void uart_puts(const char * str) {
    int i;
    for (i = 0; str[i] != '\0'; i ++)
        uart_putc(str[i]);
}

/**
 * Reads a character from UART
 * @return single character
 */
unsigned char uart_getc() {
    // Wait for UART to have received something.
    uart_flags_t flags;
    do {
        flags = read_flags();
    }
    while ( flags.recieve_queue_empty );
    return mmio_read(UART0_DR);
}

/**
 * Prints a string to UART and finishes the line
 * @param str The string to print
 */
void uart_println(const char * str) {
    uart_puts(str);
    uart_putc('\n');
}

/**
 * Print formatted string to UART
 * @param fmt currently only supports %%, %d, %f, %s without modifiers
 * @param ... values to be rendered
 */
void uart_printf(const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt != '\0'; fmt++) {
        if (*fmt == '%') {
            switch (*(++fmt)) {
                case '%':
                    uart_putc('%');
                    break;
                case 'd':
                    uart_puts(itoa(va_arg(args, int), 10));
                    break;
                case 'x':
                    uart_puts(itoa(va_arg(args, int), 16));
                    break;
                case 's':
                    uart_puts(va_arg(args, char *));
                    break;
            }
        } else uart_putc(*fmt);
    }

    va_end(args);
}