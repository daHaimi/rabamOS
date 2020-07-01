#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <kernel/timer.h>
#include <kernel/mutex.h>
#include <kernel/rand.h>
#include <kernel/gpu.h>
#include <kernel/process.h>
#include <common/logo.h>
#include <common/stdlib.h>
#include <lib/a4988.h>
#include <lib/nema.h>

uint16_t led_state = 1;
uint8_t mutex = 0;
motor_t * motor;

uint8_t endstop_x_low = 22;
uint8_t endstop_x_high = 27;

void runTilStop() {
    if (mutex) {
        return;
    }
    uint8_t endStopReached = 0;
    mutex = 1;
    motor_unpause(motor);
    do {
        if (motor->pin_status->dir == DIR_BACKWARD && gpio_read(endstop_x_low) == LOW) {
            motor_set_dir(motor, DIR_FORWARD);
            endStopReached = 1;
        }
        if (motor->pin_status->dir == DIR_FORWARD && gpio_read(endstop_x_high) == LOW) {
            motor_set_dir(motor, DIR_BACKWARD);
            endStopReached = 1;
        }
        motor_step(motor);
    } while (! endStopReached);
    motor_pause(motor);
    mutex = 0;
}

void secondly() {
    for (int i = 0; i < 5; ++i) {
        uart_putc('X');
        udelay(1000000);
    }
}

void setup() {
    init_gpio();

    // LED
    gpio_mode_pwm(GPIO18);
    gpio_write_analog(GPIO18, led_state);

    // Button
    gpio_mode(GPIO17, GPIO_MODE_IN);
    gpio_set_pull(GPIO17, HIGH);
    gpio_interrupt(GPIO17, EVENTS_FALLING, runTilStop);

    // Endstops
    gpio_mode(endstop_x_low, GPIO_MODE_IN);
    gpio_mode(endstop_x_high, GPIO_MODE_IN);

    motor = init_motor(23, 24, 25, STEPPER_RESOLUTION);
    motor->config->microstepping = MS_DIV_16; // Microstepping: 16
    motor_turn_steps(motor, STEPPER_RESOLUTION);
    motor_set_dir(motor, DIR_BACKWARD);
    motor_turn_steps(motor, STEPPER_RESOLUTION);
    motor_set_dir(motor, DIR_FORWARD);
}

void loop() {
    udelay(10);
}