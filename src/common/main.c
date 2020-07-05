#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <kernel/timer.h>
#include <kernel/mutex.h>
#include <kernel/rand.h>
#include <kernel/gpu.h>
#include <kernel/process.h>
#include <kernel/block.h>
#include <kernel/emmc.h>
#include <common/logo.h>
#include <common/stdlib.h>
#include <lib/a4988.h>
#include <lib/nema.h>

uint16_t led_state = 1;
mutex_t * mutex;
motor_t * motor;
struct block_device *sd_dev = NULL;

uint8_t endstop_x_low = 22;
uint8_t endstop_x_high = 27;

void runTilStop() {
    mutex_lock(mutex);
    uint8_t endStopReached = 0;
    uart_println("Running til endstop");
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
    uart_println("Running til endstop DONE");
    mutex_unlock(mutex);
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

    // SD Card
    if(sd_card_init(&sd_dev) == 0) {
        uart_println("Successfully initialized SD card");
    }

    // Endstops
    gpio_mode(endstop_x_low, GPIO_MODE_IN);
    gpio_mode(endstop_x_high, GPIO_MODE_IN);

    motor = init_motor(23, 24, 25, STEPPER_RESOLUTION);
    motor->config->microstepping = MS_DIV_16; // Microstepping: 16
    motor_turn_steps(motor, STEPPER_RESOLUTION);
    motor_set_dir(motor, DIR_BACKWARD);
    motor_turn_steps(motor, STEPPER_RESOLUTION);
    motor_set_dir(motor, DIR_FORWARD);

    mutex_init(mutex);
}

void loop() {
    uart_putc('.');
    udelay(100000);
}