#include <stdint.h>
#include <kernel/peripheral.h>
#ifndef TIMER_H
#define TIMER_H

#define SYSTEM_TIMER_BASE (SYSTEM_TIMER_OFFSET + PERIPHERAL_BASE)
#define TIMER_CLO         0x4

typedef unsigned int useconds_t;
struct timer_wait {
    uint32_t trigger_value;
    int rollover;
};


void timer_init(void);

void timer_set(useconds_t usecs);
void udelay(useconds_t usecs);
struct timer_wait register_timer(useconds_t usec);
int compare_timer(struct timer_wait tw);
useconds_t uuptime();

typedef struct {
    uint8_t timer0_matched: 1;
    uint8_t timer1_matched: 1;
    uint8_t timer2_matched: 1;
    uint8_t timer3_matched: 1;
    uint32_t reserved: 28;
} timer_control_reg_t;

typedef struct {
    timer_control_reg_t control;
    uint32_t counter_low;
    uint32_t counter_high;
    uint32_t timer0;
    uint32_t timer1;
    uint32_t timer2;
    uint32_t timer3;
} timer_registers_t;

#define TIMEOUT_WAIT(stop_if_true, usec)         \
do {							                 \
	struct timer_wait tw = register_timer(usec); \
	do {						                 \
		if(stop_if_true)			             \
			break;				                 \
	} while(!compare_timer(tw));			     \
} while(0);

#endif
