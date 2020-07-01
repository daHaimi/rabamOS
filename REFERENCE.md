# rabamOS Reference guide

## General kernel functions
> Todo

Based on [jsandler18's tutorial](https://github.com/jsandler18/raspi-kernel/)

## General IO via UART
For debugging purposes, a [UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter) serial adapter can be connected to GPIO pins 14 (TxD0) and 15 (RxD0). 
The following functions included from `<kernel/uart.h>` allow the communication over UART.

### void uart_putc(unsigned char c)
Put a single character to the UART output queue.

### void uart_puts(const char* str)
Put a string to the UART output queue.

### void uart_println(const char * str)
Put a string to the UART output queue (with newline character added to the end).

### void uart_printf(const char * fmt, ...)
Put a formated string to the UART output queue. The implementation is very basic, so please make sure that

* Only one format modifier is used per call
* There are only `%d`, `%x`, and `%x` implemented without modifiers.

### unsigned char uart_getc()
Read a character from the UART input.

## "stdlib"
There are some minimal reimplementations of stdlib functions included from `<common/stdlib.h>`.

### void memcpy(void * dest, const void * src, int bytes)
Copies `bytes` bytes from `src` to `dest` pointer.

### void bzero(void * dest, int bytes)
Sets `bytes` bytes to zero starting from `dest`.

### void memset(void * dest, uint8_t c, int bytes)
Sets `bytes` bytes to `c` starting from `dest`.

### char * itoa(int i, int base)
Get a string representation of an integer to a base `base`.
Special bases get a string prefixed by special markers:

| Example | Result |
| ------- | ----------- |
| itoa(17, 10) | `"17"` |
| itoa(17, 2) | `"0b10001"` |
| itoa(17, 8) | `"021"` |
| itoa(17, 16) | `"0x11"` |

### int atoi(char * num)
Parses a number out of a string.

## GPIO
Functions included in `<kernel/gpio.h>` to allow simple handling of [GPIO](https://en.wikipedia.org/wiki/General-purpose_input/output) pins.

### void init_gpio(void)
Initializes the [PWM](https://en.wikipedia.org/wiki/Pulse-width_modulation) clock. Needs to be called before other PWM functions are used.

### void gpio_mode(uint8_t gpio, uint32_t mode)
Set the mode of pin `gpio`. Valid modes are:

| Constant | Description |
| -------- | ----------- |
| GPIO_MODE_IN | Use the GPIO for reading |
| GPIO_MODE_OUT | Use the GPIO for writing |
| GPIO_MODE_ALT0 | Use the GPIO in alternative mode 0 |
| GPIO_MODE_ALT1 | Use the GPIO in alternative mode 1 |
| GPIO_MODE_ALT2 | Use the GPIO in alternative mode 2 |
| GPIO_MODE_ALT3 | Use the GPIO in alternative mode 3 |
| GPIO_MODE_ALT4 | Use the GPIO in alternative mode 4 |
| GPIO_MODE_ALT5 | Use the GPIO in alternative mode 5 |

### void gpio_mode_pwm(uint8_t gpio)
Set pin `gpio` to PWM mode. Automatically determines of the chosen pin is capable of harware PWM.

> Currently no DMA for PWM is used! So only GPIOs 12, 13, 18, 19 can be used.

### void gpio_write(uint8_t gpio, uint8_t v)
Set the state of the chosen pin. Valid states are:

| Constant | Description |
| -------- | ----------- |
| LOW | Set state to low |
| HIGH | Set state to high |

### void gpio_write_analog(uint8_t gpio, uint16_t v)
Set the analog value on a chosen PWM pin. Valid values are between `0` and `1024` by default.

### void gpio_clr(uint8_t gpio)
Unset the state of a gpio pin. Equals stting the state to `LOW`.

### uint8_t gpio_read(uint8_t gpio)
Read in the current state of a pin in `GPIO_MODE_IN` mode. May result in `LOW` or `HIGH`.

### void gpio_set_pull(uint8_t gpio, uint8_t v)
set Pull-up/-down on a pin in mode `GPIO_MODE_IN`. Valid Pull modes are:

| Constant | Description |
| -------- | ----------- |
| LOW | Set pin to pull-down |
| HIGH | Set pin to pull-up |

### void gpio_interrupt(uint8_t gpio, uint8_t mode, gpio_handler func)
Attach a function call to a flank changing event on a pin in GPIO_MODE_IN` mode. Valid events are;

| Constant | Description |
| -------- | ----------- |
| EVENTS_NONE | Disable listening for the event |
| EVENTS_FALLING | Listen for falling flank events (failsafe, listen for `HIGH`-`LOW`-`LOW`) |
| EVENTS_RISING | | Listen for rising flank events (failsafe, listen for `LOW`-`HIGH`-`HIGH`) |
| EVENTS_BOTH | Listen for rising and falling flank events (both failsafe) |
