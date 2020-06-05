#include <stddef.h>
#include <stdint.h>
#include <kernel/uart.h>
#include <common/stdlib.h>
#include <stdarg.h>

/**
 * Sets up the UART interface UART0 for reading and UART1 for Writing.
 * The default setting works with a data rate of 115200 Baud
 */
void uart_init() {
    uart_control_t control;
    // Disable UART0.
    bzero(&control, 4);
    mmio_write(UART0_CR, control.as_int);

    // Setup the GPIO pin 14 && 15.
    // Disable pull up/down for all GPIO pins & delay for 150 cycles.
    mmio_write(GPPUD, 0x00000000);
    delay(150);

    // Disable pull up/down for pin 14,15 & delay for 150 cycles.
    mmio_write(GPPUDCLK0, (1 << GPIO14) | (1 << GPIO15));
    delay(150);

    // Write 0 to GPPUDCLK0 to make it take effect.
    mmio_write(GPPUDCLK0, 0x00000000);

    // Clear pending interrupts.
    mmio_write(UART0_ICR, 0x7FF);

    // Set integer & fractional part of baud rate.
    // Divider = UART_CLOCK/(16 * Baud)
    // Fraction part register = (Fractional part * 64) + 0.5
    // UART_CLOCK = 3000000; Baud = 115200.

    // Divider = 3000000 / (16 * 115200) = 1.627 = ~1.
    mmio_write(UART0_IBRD, 1);
    // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
    mmio_write(UART0_FBRD, 40);

    // Enable FIFO & 8 bit data transmission (1 stop bit, no parity).
    mmio_write(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));

    // Mask all interrupts.
    mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) |
            (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));

    // Enable UART0, receive & transfer part of UART.
    control.uart_enabled = 1;
    control.transmit_enabled = 1;
    control.receive_enabled = 1;
    mmio_write(UART0_CR, control.as_int);
}

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
    // Wait for UART to become ready to transmit.

    do {
        flags = read_flags();
    }
    while ( flags.transmit_queue_full );
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