#include <kernel/peripheral.h>

#ifndef RABAMOS_RAND_H
#define RABAMOS_RAND_H

#define RNG_BASE        (PERIPHERAL_BASE + RNG_OFFSET)
#define RNG_CTRL        ((volatile uint32_t*)(RNG_BASE + 0x00))
#define RNG_STATUS      ((volatile uint32_t*)(RNG_BASE + 0x04))
#define RNG_DATA        ((volatile uint32_t*)(RNG_BASE + 0x08))
#define RNG_INT_MASK    ((volatile uint32_t*)(RNG_BASE + 0x10))

uint64_t rand(uint64_t min, uint64_t max);
uint8_t prandChar();

#endif //RABAMOS_RAND_H
