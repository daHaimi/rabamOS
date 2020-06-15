#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <common/stdlib.h>

void gpio_mode(uint8_t gpio, uint32_t mode) {
    // GPSEL1 is the first offset, so its offset is the offeset per selection number
    // The used registers on raspi are 0, 1 and 2
    uint32_t reg = GPFSEL0 + ((gpio / 10) * 0x4);
    // Clear the GPIOs current mode (Set to input)
    // One setting is always 3 bits long
    *(volatile uint32_t*)reg &= ~(7 << ((gpio % 10) * 3));
    if (mode > 0) {
        *(volatile uint32_t*)reg |=  (mode << ((gpio % 10) * 3));
    }
    uart_printf("Setting register %s to ", itoa(reg, 16));
    uart_printf("%s\n\r", itoa(mode << ((gpio % 10) * 3), 2));
}

void gpio_write(uint8_t gpio, uint8_t v) {
    *(volatile uint32_t*)(v == LOW ? GPCLR0 : GPSET0) = 1 << gpio;
}

void gpio_clr(uint8_t gpio) {
    *(volatile uint32_t*)GPCLR0 = 1 << gpio;
}

/**
 * Get current GPIO level
 * @param gpio
 * @return LOW or HIGH
 */
uint8_t gpio_read(uint8_t gpio) {
    delay(1);
    return (*(volatile uint32_t*)GPLEV0 >> gpio) & 1;
}

/**
 * Set Pull-Up or pull down
 * The Sequence is taken directly from the manual
 * @param gpio
 * @param v LOW (Pull-Down) or HIGH (Pull-Up)
 */
void gpio_set_pull(uint8_t gpio, uint8_t v) {
    *(volatile uint32_t*)GPPUD = 1 << v;
    delay(150);
    *(volatile uint32_t*)GPPUDCLK0 = 1 << gpio;
    delay(150);
    *(volatile uint32_t*)GPPUD = 0;
    *(volatile uint32_t*)GPPUDCLK0 = 0;
}

// TODO: Next step would be a simple way to trigger GPIO-Interrupts.
// The basic Idea is rolled out here: https://www.raspberrypi.org/forums/viewtopic.php?t=248813