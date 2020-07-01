#include <kernel/mem.h>
#include <kernel/gpio.h>
#include <kernel/timer.h>
#include <kernel/uart.h>
#include <common/stdlib.h>
#include <lib/a4988.h>

motor_t * init_motor(uint8_t pin_step, uint8_t pin_dir, uint8_t pin_sleep, uint16_t resolution) {
    motor_t * motor = (motor_t *)kmalloc(sizeof(motor_t));
    // Motor configuration
    motor->config = (motor_config_t *)kmalloc(sizeof(motor_config_t));
    motor->config->steps_round = resolution;
    motor->config->delay = 50; // us
    motor->config->microstepping = MS_DIV_1;

    // GPIO Configuration
    motor->pin_step = pin_step;
    gpio_mode(motor->pin_step, GPIO_MODE_OUT);
    motor->pin_dir = pin_dir;
    gpio_mode(motor->pin_dir, GPIO_MODE_OUT);
    motor->pin_sleep = pin_sleep;
    gpio_mode(motor->pin_sleep, GPIO_MODE_OUT);

    // Initial Status
    motor->pin_status = (pin_status_t *)kmalloc(sizeof(pin_status_t));
    gpio_write(motor->pin_step, LOW);
    motor->pin_status->dir = DIR_FORWARD;
    gpio_write(motor->pin_dir, motor->pin_status->dir);
    motor->pin_status->sleep = HIGH;
    gpio_write(motor->pin_sleep, motor->pin_status->sleep);

    return motor;
}

void motor_step(motor_t *motor) {
    gpio_write(motor->pin_step, HIGH);
    udelay(motor->config->delay);
    gpio_write(motor->pin_step, LOW);
    udelay(motor->config->delay);
}

void motor_pause(motor_t *motor) {
    motor->pin_status->sleep = LOW;
    gpio_write(motor->pin_sleep, motor->pin_status->sleep);
}

void motor_unpause(motor_t *motor) {
    motor->pin_status->sleep = HIGH;
    gpio_write(motor->pin_sleep, motor->pin_status->sleep);
}

void motor_set_dir(motor_t *motor, uint8_t dir) {
    motor->pin_status->dir = dir;
    gpio_write(motor->pin_dir, motor->pin_status->dir);
}

void motor_switch_dir(motor_t *motor) {
    motor_set_dir(motor, ! motor->pin_status->dir);
}

void motor_turn_steps(motor_t *motor, uint64_t steps) {
    motor_unpause(motor);
    for (uint64_t i = 0; i < steps * motor->config->microstepping; i++) {
        motor_step(motor);
    }
    motor_pause(motor);
}

void motor_turn_steps_in_usec(motor_t *motor, uint64_t steps, uint32_t usec) {
    if (usec < steps * 2) {
        uart_puts("ERROR: Not possible to sleep less than 1 us");
        return;
    }
    uint32_t pause = div(usec, steps * 2);
    motor_unpause(motor);
    for (uint64_t i = 0; i < steps * motor->config->microstepping; i++) {
        uint32_t budget = uuptime();
        gpio_write(motor->pin_step, HIGH);
        udelay(pause);
        gpio_write(motor->pin_step, LOW);
        budget = uuptime() - (budget + pause);
        udelay(budget);
    }
    motor_pause(motor);
}