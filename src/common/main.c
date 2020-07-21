#include <kernel/gpio.h>
#include <kernel/kerio.h>
#include <kernel/uart.h>
#include <kernel/timer.h>
#include <kernel/mutex.h>
#include <kernel/rand.h>
#include <kernel/gpu.h>
#include <kernel/process.h>
#include <kernel/emmc.h>
#include <kernel/fs.h>
#include <kernel/vfs.h>
#include <common/logo.h>
#include <common/stdlib.h>
#include <common/2d.h>
#include <lib/a4988.h>
#include <lib/nema.h>

uint16_t led_state = 1;
mutex_t * mutex;
motor_t * motor_x;
motor_t * motor_y;

uint8_t endstop_x_low = 42; // FAKE
uint8_t endstop_x_high = 47; // FAKE
uint8_t endstop_y_low = 22;
uint8_t endstop_y_high = 27;
point2d_t * size_steps;

// Forward declarations
uint64_t taraSteps(motor_t * motor, uint8_t endstop_low, uint8_t endstop_high);
void home();

void runTilStop() {
    mutex_lock(mutex);
    motor_set_dir(motor_y, DIR_BACKWARD);
    motor_unpause(motor_y);
    motor_step(motor_y);
    motor_pause(motor_y);
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
    struct block_device * sdcard = libfs_init();
    uart_println("Initialized SD card");
    vfs_register(sdcard->fs);
    //register_fs(sdcard, 1);
    vfs_list_devices();

    /*
    FRESULT rc = pf_mount(&sdcard);
    if (rc) {
        uart_printf("Mounting SD card failed with code %d\n", rc);
        return;
    }

    uart_printf("Mounted SD card '%s' with driver '%s' [%d blocks a %d]\n", get_device()->device_name, get_device()->driver_name, get_device()->num_blocks, get_device()->block_size);
    uart_printf("Root folder starting at '%x'\n", sdcard.dirbase);

    char * cfgfn = "config.txt";
    rc = pf_open(cfgfn);
    if (rc) {
        uart_printf("Failed opening %s with: %d\n", cfgfn, rc);
        return;
    }
    for (;;) {
        rc = pf_read(buff, sizeof(buff), &br);	// Read a chunk of file
        if (rc || !br) break;			// Error or end of file
        for (i = 0; i < br; i++)		// Type the data
            uart_putc(buff[i]);
    }
    if (rc) {
        uart_printf("Failed reading from %s with: %d\n", cfgfn, rc);
        return;
    }

    /*
rc = pf_opendir(&dir, "");
if (rc) {
    uart_printf("Listing SD card content failed: %d\n", rc);
    return;
}

uart_printf("\nDirectory listing...\n");
uint8_t elems = 0;
for (;;) {
    rc = pf_readdir(&dir, &fno);	// Read a directory item
    if (rc || !fno.fname[0]) break;	// Error or end of dir
    if (fno.fattrib & AM_DIR)
        uart_printf("   <dir>  %s\n", fno.fname);
    else
        uart_printf("%d  %s\n", fno.fsize, fno.fname);
    elems++;
}
if (rc) {
    uart_printf("Iterating SD card failed with code %d\n", rc);
    return;
}
uart_printf("Directory contains %d elements\n", elems);

        */
    // Endstops
    gpio_mode(endstop_y_low, GPIO_MODE_IN);
    gpio_mode(endstop_y_high, GPIO_MODE_IN);

    motor_y = init_motor(23, 24, 25, STEPPER_RESOLUTION);
    motor_y->config->microstepping = MS_DIV_16; // Microstepping: 16


    printf("Tare...\n");
    size_steps->x = 0;
    //size_steps->x = taraSteps(motor_x, endstop_x_low, endstop_x_high);
    printf("X-Size: %d steps\n", size_steps->x);
    size_steps->y = taraSteps(motor_y, endstop_y_low, endstop_y_high);
    printf("Y-Size: %d steps\n", size_steps->y);
    printf("homing...\n");
    home();

    mutex_init(mutex);
}

uint64_t taraSteps(motor_t * motor, uint8_t endstop_low, uint8_t endstop_high) {
    motor_set_dir(motor, DIR_BACKWARD); // Run backwards until lower endstop
    motor_unpause(motor);
    while (gpio_read(endstop_low) == HIGH) {
        motor_step(motor);
    }
    motor_set_dir(motor_y, DIR_FORWARD);
    // Now begin the counting
    uint64_t steps = 0;
    while (gpio_read(endstop_high) == HIGH) {
        motor_step(motor);
        steps++;
    }
    motor_pause(motor);
    return steps;
}

void home() {
    point2d_t phome = {
        x: size_steps->x / 2,
        y: size_steps->y / 2,
    };
    motor_unpause(motor_x);
    motor_unpause(motor_y);
    motor_set_dir(motor_x, DIR_BACKWARD);
    motor_set_dir(motor_y, DIR_BACKWARD);
    uint8_t stepped;
    while (1) {
        stepped = 1;
        if (gpio_read(endstop_x_low) == HIGH) {
            motor_step(motor_x);
            stepped <<= 1;
        }
        if (gpio_read(endstop_y_low) == HIGH) {
            motor_step(motor_y);
            stepped <<= 1;
        }
        if (stepped == 1) {
            break;
        }
    }
    motor_set_dir(motor_x, DIR_FORWARD);
    motor_set_dir(motor_y, DIR_FORWARD);
    uint64_t x = 0, y = 0;
    while (1) {
        stepped = 1;
        if (++x >= phome.y) {
            motor_step(motor_x);
            stepped <<= 1;
        }
        if (++y >= phome.y) {
            motor_step(motor_y);
            stepped <<= 1;
        }
        if (stepped == 1) {
            break;
        }
    }
    motor_pause(motor_x);
    motor_pause(motor_y);
}

void loop() {
    putc('.');
    udelay(100000);
}

// Realtime ticks
void tick() {
    return;
}