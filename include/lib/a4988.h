#ifndef RABAMOS_A4988_H
#define RABAMOS_A4988_H

#define MS_DIV_1 1
#define MS_DIV_2 2
#define MS_DIV_4 4
#define MS_DIV_8 8
#define MS_DIV_16 16

#define DIR_FORWARD 0
#define DIR_BACKWARD 1

typedef struct {
    uint16_t steps_round;
    uint16_t delay;
    uint8_t microstepping;
} motor_config_t;

typedef struct {
    uint8_t dir: 1;
    uint8_t sleep: 1;
} pin_status_t;

typedef struct {
    uint8_t microstepping;
    uint8_t pin_step;
    uint8_t pin_dir;
    uint8_t pin_sleep;
    pin_status_t * pin_status;
    motor_config_t * config;
} motor_t;

motor_t * init_motor(uint8_t pin_step, uint8_t pin_dir, uint8_t pin_sleep, uint16_t resolution);
void motor_step(motor_t *motor);
void motor_pause(motor_t *motor);
void motor_unpause(motor_t *motor);
void motor_set_dir(motor_t *motor, uint8_t dir);
void motor_switch_dir(motor_t *motor);
void motor_turn_steps(motor_t *motor, uint64_t steps);
void motor_turn_steps_in_usec(motor_t *motor, uint64_t steps, uint32_t usec);

#endif