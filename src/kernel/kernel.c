#include <stddef.h>
#include <stdint.h>
#include <kernel/uart.h>
#include <kernel/mem.h>
#include <kernel/atag.h>
#include <kernel/kerio.h>
#include <kernel/gpu.h>
#include <kernel/interrupts.h>
#include <kernel/timer.h>
#include <kernel/process.h>
#include <kernel/mutex.h>
#include <kernel/uart.h>
#include <common/stdlib.h>
#include <common/main.h>


void main_loop() {
    while (1) {
        loop();
        tick();
    }
}

void rt_loop() {
    while (1) {
        tick();
    }
}

void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags) {
    // Declare as unused
    (void) r0;
    (void) r1;
    (void) atags;

    mem_init((atag_t *)atags);
    gpu_init();
    uart_puts("GPU INITIALIZED . ");

    uart_puts("INTERRUPTS");
    interrupts_init();
    uart_puts(". ");
    uart_puts("TIMER ");
    timer_init();
    uart_puts(". ");
    uart_puts("SCHEDULER ");
    process_init();
    uart_puts(".\n");

    uart_puts("Running setup...");
    setup();
    uart_puts("DONE\n");
    uart_printf("Kernel booted in %dms\n", uuptime() / 1000);

    main_loop();
    //create_kernel_thread(main_loop, "MAIN", 4);
    //create_kernel_thread(rt_loop, "RTLOOP", 6);
}