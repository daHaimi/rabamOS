#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <common/stdlib.h>

uint8_t button_state = LOW;

void setup() {
    // LED
    gpio_mode(GPIO4, GPIO_MODE_OUT);
    gpio_write(GPIO4, LOW);
    // Button
    gpio_mode(GPIO17, GPIO_MODE_IN);
    gpio_set_pull(GPIO17, HIGH);
}

void loop() {
    button_state = button_state == LOW ? HIGH : LOW;
    /*
    if (gpio_read(GPIO17)) {
        button_state = HIGH;
    }
    */
    gpio_write(GPIO4, button_state);
    uart_printf("GPIO IN register: %s\n", itoa(mmio_read(GPLEV0), 2));
    uart_printf("Button state: %d\n", gpio_read(GPIO17));
}