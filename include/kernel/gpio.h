#include <kernel/peripheral.h>

#ifndef GPIO_H
#define GPIO_H

// HIGH ansd LOW constants
#define LOW 0
#define HIGH 1

// Define available GPIOs
#define GPIO2   2 /* SDA1 */
#define GPIO3   3 /* SCL1 */
#define GPIO4   4
#define GPIO5   5
#define GPIO6   6
#define GPIO7   7 /* CE1 */
#define GPIO8   8 /* CE0 */
#define GPIO9   9 /* MISO */
#define GPIO10 10 /* MOSI */
#define GPIO11 11 /* SCLK */
#define GPIO12 12
#define GPIO13 13
#define GPIO14 14 /* TxD */
#define GPIO15 15 /* RxD */
#define GPIO16 16
#define GPIO17 17
#define GPIO18 18 /* PCM_CLK */
#define GPIO19 19
#define GPIO20 20
#define GPIO21 21
#define GPIO22 22
#define GPIO23 23
#define GPIO24 24
#define GPIO25 25
#define GPIO26 26
#define GPIO27 27

// The GPIO registers base address.
#define GPIO_BASE (PERIPHERAL_BASE + GPIO_OFFSET)
#define GPFSEL0 (GPIO_BASE + 0x00) /* GPIO Function Selection 0 */
#define GPFSEL1 (GPIO_BASE + 0x04) /* GPIO Function Selection 1 */
#define GPFSEL2 (GPIO_BASE + 0x08) /* GPIO Function Selection 2 */
#define GPSET0 (GPIO_BASE + 0x1C)  /* Set GPIO value */
#define GPCLR0 (GPIO_BASE + 0x28)  /* Clear GPIO value */
#define GPLEV0 (GPIO_BASE + 0x2C)  /* Get GPIO value */
#define GPEDS0 (GPIO_BASE + 0x40)  /* Event detect status */
#define GPREN0 (GPIO_BASE + 0x4C)  /* Rising Edge detection enable */
#define GPFEN0 (GPIO_BASE + 0x58)  /* Falling Edge detection enable */
#define GPHEN0 (GPIO_BASE + 0x64)  /* Pin HIGH detection enable */
#define GPLEN0 (GPIO_BASE + 0x70)  /* Pin LOW detection enable */
#define GPAREN0 (GPIO_BASE + 0x7C) /* Async rising edge detection */
#define GPAFEN0 (GPIO_BASE + 0x88) /* Async falling edge detection */
#define GPPUD (GPIO_BASE + 0x94) /* Controls actuation of pull up/down to ALL GPIO pins. */
#define GPPUDCLK0 (GPIO_BASE + 0x98)/* Controls actuation of pull up/down for specific GPIO pin. */

// GPIO Modes
#define GPIO_MODE_IN   = 0
#define GPIO_MODE_OUT  = 1
#define GPIO_MODE_ALT0 = 4
#define GPIO_MODE_ALT1 = 5
#define GPIO_MODE_ALT2 = 6
#define GPIO_MODE_ALT3 = 7
#define GPIO_MODE_ALT4 = 3
#define GPIO_MODE_ALT5 = 2

// Define events for interrupts
#define EVENTS_NONE    0
#define EVENTS_FALLING 1
#define EVENTS_RISING  2
#define EVENTS_BOTH    3


// Macros
void gpio_mode(uint8_t gpio, char mode);
void gpio_write(uint8_t gpio, uint8_t v);
void gpio_clr(uint8_t gpio);
uint8_t gpio_read(uint8_t gpio);
void gpio_set_pull(uint8_t gpio, uint8_t v);

#endif
