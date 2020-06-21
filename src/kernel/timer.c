#include <kernel/timer.h>
#include <kernel/process.h>
#include <kernel/interrupts.h>
#include <kernel/kerio.h>

static timer_registers_t * timer_regs;

static void timer_irq_handler(void) {
    schedule();
}

static void timer_irq_clearer(void) {
    timer_regs->control.timer1_matched = 1;
}

void timer_init(void) {
    timer_regs = (timer_registers_t *) SYSTEM_TIMER_BASE;
    register_irq_handler(SYSTEM_TIMER_1, timer_irq_handler, timer_irq_clearer);
}

void timer_set(useconds_t usecs) {
    timer_regs->timer1 = timer_regs->counter_low + usecs;
}

__attribute__ ((optimize(0))) void udelay (useconds_t usecs) {
    volatile uint32_t curr = timer_regs->counter_low;
    volatile uint32_t x = timer_regs->counter_low - curr;
    while (x < usecs) {
        x = timer_regs->counter_low - curr;
    }
}



struct timer_wait register_timer(useconds_t usec) {
    struct timer_wait tw;
    tw.rollover = 0;
    tw.trigger_value = 0;
    uint32_t cur_timer = mmio_read(SYSTEM_TIMER_BASE + TIMER_CLO);
    uint32_t trig = cur_timer + (uint32_t)usec;

    if(cur_timer == 0)
        trig = 0;

    tw.trigger_value = trig;
    if(trig > cur_timer)
        tw.rollover = 0;
    else
        tw.rollover = 1;
    return tw;
}

int compare_timer(struct timer_wait tw) {
    uint32_t cur_timer = mmio_read(SYSTEM_TIMER_BASE + TIMER_CLO);
    if(tw.trigger_value == 0)
        return 1;

    if(cur_timer < tw.trigger_value) {
        if(tw.rollover)
            tw.rollover = 0;
    } else if(!tw.rollover)
        return 1;
    return 0;
}

useconds_t uuptime() {
    return timer_regs->counter_low;
}