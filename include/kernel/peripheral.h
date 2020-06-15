#include <stdint.h>

#ifndef PERIPHERAL_H
#define PERIPHERAL_H
/**
 * Model 2 and 3 have 0x3F000000 as peripheral base
 */
#define PERIPHERAL_BASE 0x20000000
#define PERIPHERAL_LENGTH 0x01000000

#define SYSTEM_TIMER_OFFSET 0x3000
#define INTERRUPTS_OFFSET 0xB000
#define MAILBOX_OFFSET 0xB880
#define UART0_OFFSET 0x201000
#define GPIO_OFFSET 0x200000
#define EMMC_OFFSET 0x300000

void mmio_write(uint32_t reg, uint32_t data);
uint32_t mmio_read(uint32_t reg);
void delay(int32_t count);

#endif
