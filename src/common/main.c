#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <kernel/timer.h>
#include <common/stdlib.h>

uint16_t led_state = 1;
uint32_t nextTimer = 0;

void iButton() {
    if (nextTimer < uuptime()) {
        return;
    }
    nextTimer = uuptime() + 100000;
    led_state <<= 1;
    if (led_state > 1024) led_state = 1;
    uart_printf("Setting PWM value to %d\n", led_state);
    gpio_write_analog(GPIO18, led_state);
}

void setup() {
    // LED
    gpio_mode_pwm(GPIO18);
    gpio_write_analog(GPIO18, led_state);
    // Button
    gpio_mode(GPIO17, GPIO_MODE_IN);
    gpio_set_pull(GPIO17, HIGH);

    gpio_interrupt(GPIO17, EVENTS_FALLING, iButton);
}

void loop() {
}