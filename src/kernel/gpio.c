#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <common/stdlib.h>


void printButton () {
    printf("Button pressed!\n");
}

void gpio_mode(uint8_t gpio, char mode) {
    uint32_t reg = gpio > 19 ? GPFSEL2 : (gpio > 9 ? GPFSEL1 : GPFSEL0);
    // read and clear the GPIOs current mode (Set to input)
    uint32_t modes = mmio_read(reg) & ~(7 << ((gpio % 10) * 3));
    if (mode > 0) {
        modes |=  (1 << ((gpio % 10) * 3));
    }
    mmio_write(reg, modes);
}

void gpio_write(uint8_t gpio, uint8_t v) {
    mmio_write(GPSET0, v << gpio);
}

void gpio_clr(uint8_t gpio) {
    mmio_write(GPCLR0, 1 << gpio);
}

/**
 * Get current GPIO level
 * @param gpio
 * @return LOW or HIGH
 */
uint8_t gpio_read(uint8_t gpio) {
    return (mmio_read(GPLEV0) >> gpio) & 1;
}

/**
 * Set Pull-Up or pull down
 * The Sequence is taken directly from the manual
 * @param gpio
 * @param v LOW (Pull-Down) or HIGH (Pull-Up)
 */
void gpio_set_pull(uint8_t gpio, uint8_t v) {
    mmio_write(GPPUD, 1 << v);
    delay(150);
    mmio_write(GPPUDCLK0, (1 << gpio));
    delay(150);
    mmio_write(GPPUD, 0x00000000);
    mmio_write(GPPUDCLK0, 0x00000000);
}