#include <kernel/mem.h>
#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <kernel/interrupts.h>
#include <kernel/process.h>
#include <kernel/timer.h>
#include <kernel/mutex.h>
#include <kernel/rand.h>
#include <common/stdlib.h>

uint8_t numGPIOInterrupts = 0;
uint32_t interruptsReceived = 0;
uint32_t launchThreads = 0x00000000;
gpio_handler gpioHandler[GPIO_NUM_HANDLERS];
uint32_t pwm_ctl = 0;

#define CLK_BASE 0x5a000000

void init_gpio() {
    // First interrupt binding to -1
    for (uint8_t i = 0; i < GPIO_NUM_HANDLERS; i++) {
        gpioHandler[i] = 0;
    }
    *(volatile uint32_t*)CM_PWMCTL = CLK_BASE | ~0x10; // Turn off enable flag.
    while(*(volatile uint32_t*)CM_PWMCTL & 0x80); // Wait for busy flag to turn off.
    *(volatile uint32_t*)CM_PWMDIV = CLK_BASE | (5 << 12); // Configure divider.
    *(volatile uint32_t*)CM_PWMCTL = CLK_BASE | 0x206; // Source=PLLD (500 MHz), 1-stage MASH.
    *(volatile uint32_t*)CM_PWMCTL = CLK_BASE | 0x216; // Enable clock.
    while(!(*(volatile uint32_t*)CM_PWMCTL & 0x80)); // Wait for busy flag to turn on.
}

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
}
void gpio_mode_pwm(uint8_t gpio) {
    if (gpio != 12 && gpio != 13 && gpio != 18 && gpio != 19) {
        // TODO: Maybe allow any pin via DMA later, for now only HW GPIO!
        uart_println("Invalid GPIO for PWM usage");
        return;
    }
    if (gpio < 14) {
        // GPIO12 & 13 have PWM as Alt Fun 0
        gpio_mode(gpio, GPIO_MODE_ALT0);
    }
    if (gpio < 20) {
        // GPIO18 & 19 have PWM as Alt Fun 5
        gpio_mode(gpio, GPIO_MODE_ALT5);
    }
    if (gpio == 13 || gpio == 18) {
        pwm_ctl |= 1; // CTL: Enable Channel 1
        *(volatile uint32_t*)PWM_RNG1 = PWM_RANGE;
    }
    if (gpio == 13 || gpio == 19) {
        pwm_ctl |= (1 << 8); // CTL: Enable channel 2
        *(volatile uint32_t*)PWM_RNG2 = PWM_RANGE;
    }
    *(volatile uint32_t*)PWM_CTL = pwm_ctl;
}

void gpio_write(uint8_t gpio, uint8_t v) {
    *(volatile uint32_t*)(v == LOW ? GPCLR0 : GPSET0) = 1 << gpio;
}

void gpio_write_analog(uint8_t gpio, uint16_t v) {
    if (gpio == 13 || gpio == 18) {
        *(volatile uint32_t*)PWM_DAT1 = v;
    }
    if (gpio == 13 || gpio == 19) {
        *(volatile uint32_t*)PWM_DAT2 = v;
    }
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
    return ((*(volatile uint32_t*)GPLEV0) >> gpio) & 1;
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

/**
 * The handler for all GPIO Events
 */
uint8_t started = 0;
static void gpio_irq_handler(void) {
    for (uint8_t gpio = 0; gpio < 32; ++gpio) {
        if (interruptsReceived & (1 << gpio)) {
            if (gpioHandler[gpio] && ! started) {
                uart_puts("{GPIO}");
                //started = 1;
                //create_kernel_thread(gpioHandler[gpio], "GPIO", 4);
                gpioHandler[gpio]();
            }
            interruptsReceived &= ~(1 << gpio);
        }
    }
}

/**
 * Clearing is done in the handler
 */
static void gpio_irq_clearer(void) {
    interruptsReceived = *(volatile uint32_t*)GPEDS0;
    *(volatile uint32_t*)GPEDS0 &= 0xFFFFFFFF;
}

/**
 * Register a GPIO interrupt and bind it o a function
 * @param gpio The GPIO number
 * @param mode The wath mode (rising/falling/both)
 * @param func The callback function
 */
void gpio_interrupt(uint8_t gpio, uint8_t mode, gpio_handler func) {
    // Falling edge
    if (mode & 1) {
        *(volatile uint32_t*)GPFEN0 |= 1 << gpio;
    }
    // Rising edge
    if (mode & 2) {
        *(volatile uint32_t*)GPREN0 |= 1 << gpio;
    }
    // Clear Event detection status
    uart_println("Resetting event...");
    *(volatile uint32_t*)GPEDS0 |= 1 << gpio;
    // For the first activation of interrupts, activate the interrupt handler (IRQ 49)
    if (numGPIOInterrupts == 0) {
        uart_println("Register interrupt handler");
        bzero(gpioHandler, sizeof(gpio_handler) * GPIO_NUM_HANDLERS);
        register_irq_handler(GPIO_IRQ, gpio_irq_handler, gpio_irq_clearer);
    }
    uart_println("Setting callback function");
    gpioHandler[gpio] = func;
}