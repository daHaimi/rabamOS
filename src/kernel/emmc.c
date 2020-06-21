#include <stdint.h>
#include <kernel/emmc.h>
#include <kernel/peripheral.h>
#include <kernel/mailbox.h>
#include <kernel/uart.h>
#include <kernel/mem.h>
#include <common/util.h>
#include <common/stdlib.h>

static void sd_power_off() {
    /* Power off the SD card */
    uint32_t control0 = mmio_read(EMMC_BASE + EMMC_CONTROL0);
    control0 &= ~(1 << 8);	// Set SD Bus Power bit off in Power Control Register
    mmio_write(EMMC_BASE + EMMC_CONTROL0, control0);
}

static uint32_t sd_get_base_clock_hz() {
    mail_message_t msg;
    uint32_t mailbuffer[8];

    /* Get the base clock rate */
    // set up the buffer
    mailbuffer[0] = 8 * 4;		// size of this message
    mailbuffer[1] = 0;			// this is a request

    // next comes the first tag
    mailbuffer[2] = 0x00030002;	// get clock rate tag
    mailbuffer[3] = 0x8;		// value buffer size
    mailbuffer[4] = 0x4;		// is a request, value length = 4
    mailbuffer[5] = 0x1;		// clock id + space to return clock id
    mailbuffer[6] = 0;			// space to return rate (in Hz)

    // closing tag
    mailbuffer[7] = 0;

    // send the message
    msg.data = (uint32_t)mailbuffer;
    mailbox_send(msg, PROPERTY_CHANNEL);

    // read the response
    msg = mailbox_read(PROPERTY_CHANNEL);

    if (!msg.data)
        return -1;

    if(mailbuffer[1] != MBOX_SUCCESS) {
        uart_printf("EMMC: property mailbox did not return a valid response.\n");
        return 0;
    }

    if(mailbuffer[5] != 0x1) {
        uart_printf("EMMC: property mailbox did not return a valid clock id.\n");
        return 0;
    }
#ifdef EMMC_DEBUG
    uart_printf("EMMC: base clock rate is %i Hz\n", mailbuffer[6]);
#endif
    return mailbuffer[6];
}

static int bcm_2708_power_off() {
    mail_message_t msg;
    uint32_t mailbuffer[8];

    /* Power off the SD card */
    // set up the buffer
    mailbuffer[0] = 8 * 4;		// size of this message
    mailbuffer[1] = 0;			// this is a request

    // next comes the first tag
    mailbuffer[2] = 0x00028001;	// set power state tag
    mailbuffer[3] = 0x8;		// value buffer size
    mailbuffer[4] = 0x8;		// is a request, value length = 8
    mailbuffer[5] = 0x0;		// device id and device id also returned here
    mailbuffer[6] = 0x2;		// set power off, wait for stable and returns state

    // closing tag
    mailbuffer[7] = 0;

    // send the message
    msg.data = (uint32_t)mailbuffer;
    mailbox_send(msg, PROPERTY_CHANNEL);

    // read the response
    mailbox_read(PROPERTY_CHANNEL);

    if(mailbuffer[1] != MBOX_SUCCESS)
    {
        uart_printf("EMMC: bcm_2708_power_off(): property mailbox did not return a valid response.\n");
        return -1;
    }

    if(mailbuffer[5] != 0x0)
    {
        uart_printf("EMMC: property mailbox did not return a valid device id.\n");
        return -1;
    }

    if((mailbuffer[6] & 0x3) != 0)
    {
#ifdef EMMC_DEBUG
        uart_printf("EMMC: bcm_2708_power_off(): device did not power off successfully (%08x).\n", mailbuffer[6]);
#endif
        return 1;
    }

    return 0;
}

static int bcm_2708_power_on() {
    mail_message_t msg;
    uint32_t mailbuffer[8];

    /* Power on the SD card */
    // set up the buffer
    mailbuffer[0] = 8 * 4;		// size of this message
    mailbuffer[1] = 0;			// this is a request

    // next comes the first tag
    mailbuffer[2] = 0x00028001;	// set power state tag
    mailbuffer[3] = 0x8;		// value buffer size
    mailbuffer[4] = 0x8;		// is a request, value length = 8
    mailbuffer[5] = 0x0;		// device id and device id also returned here
    mailbuffer[6] = 0x3;		// set power off, wait for stable and returns state

    // closing tag
    mailbuffer[7] = 0;

    // send the message
    msg.data = (uint32_t)mailbuffer;
    mailbox_send(msg, PROPERTY_CHANNEL);

    // read the response
    mailbox_send(msg, PROPERTY_CHANNEL);

    if(mailbuffer[1] != MBOX_SUCCESS)
    {
        uart_printf("EMMC: bcm_2708_power_on(): property mailbox did not return a valid response.\n");
        return -1;
    }

    if(mailbuffer[5] != 0x0)
    {
        uart_printf("EMMC: property mailbox did not return a valid device id.\n");
        return -1;
    }

    if((mailbuffer[6] & 0x3) != 1)
    {
#ifdef EMMC_DEBUG
        uart_printf("EMMC: bcm_2708_power_on(): device did not power on successfully (%08x).\n", mailbuffer[6]);
#endif
        return 1;
    }

    return 0;
}

static int bcm_2708_power_cycle()
{
    if(bcm_2708_power_off() < 0)
        return -1;

    udelay(5000);

    return bcm_2708_power_on();
}

// Set the clock dividers to generate a target value
static uint32_t sd_get_clock_divider(uint32_t base_clock, uint32_t target_rate) {
    uint32_t targetted_divisor = 0;
    if (target_rate > base_clock) {
        targetted_divisor = 1;
    } else {
        divmod_t mod = divmod(base_clock, target_rate);
        targetted_divisor = mod.div;
        if(mod.mod)
            targetted_divisor--;
    }

    if(hci_ver >= 2) {
        // HCI version 3 or greater supports 10-bit divided clock mode
        // This requires a power-of-two divider

        // Find the first bit set
        int divisor = -1;
        for (int first_bit = 31; first_bit >= 0; first_bit--) {
            uint32_t bit_test = (1 << first_bit);
            if(targetted_divisor & bit_test) {
                divisor = first_bit;
                targetted_divisor &= ~bit_test;
                if(targetted_divisor) {
                    // The divisor is not a power-of-two, increase it
                    divisor++;
                }
                break;
            }
        }

        if(divisor == -1 || divisor >= 32)
            divisor = 31;

        if(divisor != 0)
            divisor = (1 << (divisor - 1));

        if(divisor >= 0x400)
            divisor = 0x3ff;

        uint32_t freq_select = divisor & 0xff;
        uint32_t upper_bits = (divisor >> 8) & 0x3;
        uint32_t ret = (freq_select << 8) | (upper_bits << 6) | (0 << 5);

#ifdef EMMC_DEBUG
        int denominator = 1;
            if(divisor != 0)
                denominator = divisor * 2;
            int actual_clock = div(base_clock, denominator);
            uart_printf("EMMC: base_clock: %i, target_rate: %i, divisor: %08x, "
                   "actual_clock: %i, ret: %08x\n", base_clock, target_rate,
                   divisor, actual_clock, ret);
#endif

        return ret;
    } else {
        uart_printf("EMMC: unsupported host version\n");
        return SD_GET_CLOCK_DIVIDER_FAIL;
    }
}

// Switch the clock rate whilst running
static int sd_switch_clock_rate(uint32_t base_clock, uint32_t target_rate) {
    // Decide on an appropriate divider
    uint32_t divider = sd_get_clock_divider(base_clock, target_rate);
    if(divider == SD_GET_CLOCK_DIVIDER_FAIL) {
        uart_printf("EMMC: couldn't get a valid divider for target rate %i Hz\n", target_rate);
        return -1;
    }

    // Wait for the command inhibit (CMD and DAT) bits to clear
    while(mmio_read(EMMC_BASE + EMMC_STATUS) & 0x3)
        udelay(1000);

    // Set the SD clock off
    uint32_t control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
    control1 &= ~(1 << 2);
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    udelay(2000);

    // Write the new divider
    control1 &= ~0xffe0; // Clear old setting + clock generator select
    control1 |= divider;
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    udelay(2000);

    // Enable the SD clock
    control1 |= (1 << 2);
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    udelay(2000);

#ifdef EMMC_DEBUG
    uart_printf("EMMC: successfully set clock rate to %i Hz\n", target_rate);
#endif
    return 0;
}

// Reset the CMD line
static int sd_reset_cmd() {
    uint32_t control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
    control1 |= SD_RESET_CMD;
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    TIMEOUT_WAIT((mmio_read(EMMC_BASE + EMMC_CONTROL1) & SD_RESET_CMD) == 0, 1000000);
    if((mmio_read(EMMC_BASE + EMMC_CONTROL1) & SD_RESET_CMD) != 0) {
        uart_printf("EMMC: CMD line did not reset properly\n");
        return -1;
    }
    return 0;
}

// Reset the CMD line
static int sd_reset_dat() {
    uint32_t control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
    control1 |= SD_RESET_DAT;
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    TIMEOUT_WAIT((mmio_read(EMMC_BASE + EMMC_CONTROL1) & SD_RESET_DAT) == 0, 1000000);
    if((mmio_read(EMMC_BASE + EMMC_CONTROL1) & SD_RESET_DAT) != 0) {
        uart_printf("EMMC: DAT line did not reset properly\n");
        return -1;
    }
    return 0;
}

static void sd_issue_command_int(struct emmc_block_dev *dev, uint32_t cmd_reg, uint32_t argument, useconds_t timeout) {
    dev->last_cmd_reg = cmd_reg;
    dev->last_cmd_success = 0;

    // This is as per HCSS 3.7.1.1/3.7.2.2
    // Check Command Inhibit
    while(mmio_read(EMMC_BASE + EMMC_STATUS) & 0x1)
        udelay(1000);

    // Is the command with busy?
    if((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) {
        // With busy
        // Is is an abort command?
        if((cmd_reg & SD_CMD_TYPE_MASK) != SD_CMD_TYPE_ABORT) {
            // Not an abort command
            // Wait for the data line to be free
            while(mmio_read(EMMC_BASE + EMMC_STATUS) & 0x2)
                udelay(1000);
        }
    }

    // Is this a DMA transfer?
    int is_sdma = 0;
    if((cmd_reg & SD_CMD_ISDATA) && (dev->use_sdma)) {
#ifdef EMMC_DEBUG
        uart_printf("SD: performing SDMA transfer, current INTERRUPT: %08x\n", mmio_read(EMMC_BASE + EMMC_INTERRUPT));
#endif
        is_sdma = 1;
    }

    // Set block size and block count
    // For now, block size = 512 bytes, block count = 1,
    //  host SDMA buffer boundary = 4 kiB
    if(dev->blocks_to_transfer > 0xffff) {
        uart_printf("SD: blocks_to_transfer too great (%i)\n", dev->blocks_to_transfer);
        dev->last_cmd_success = 0;
        return;
    }
    uint32_t blksizecnt = dev->block_size | (dev->blocks_to_transfer << 16);
    mmio_write(EMMC_BASE + EMMC_BLKSIZECNT, blksizecnt);

    // Set argument 1 reg
    mmio_write(EMMC_BASE + EMMC_ARG1, argument);

    if(is_sdma) {
        // Set Transfer mode register
        cmd_reg |= SD_CMD_DMA;
    }

    // Set command reg
    mmio_write(EMMC_BASE + EMMC_CMDTM, cmd_reg);

    udelay(2000);

    // Wait for command complete interrupt
    TIMEOUT_WAIT(mmio_read(EMMC_BASE + EMMC_INTERRUPT) & 0x8001, timeout);
    uint32_t irpts = mmio_read(EMMC_BASE + EMMC_INTERRUPT);

    // Clear command complete status
    mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff0001);

    // Test for errors
    if((irpts & 0xffff0001) != 0x1) {
#ifdef EMMC_DEBUG
        uart_printf("SD: error occured whilst waiting for command complete interrupt\n");
#endif
        dev->last_error = irpts & 0xffff0000;
        dev->last_interrupt = irpts;
        return;
    }

    udelay(2000);

    // Get response data
    switch(cmd_reg & SD_CMD_RSPNS_TYPE_MASK) {
        case SD_CMD_RSPNS_TYPE_48:
        case SD_CMD_RSPNS_TYPE_48B:
            dev->last_r0 = mmio_read(EMMC_BASE + EMMC_RESP0);
            break;

        case SD_CMD_RSPNS_TYPE_136:
            dev->last_r0 = mmio_read(EMMC_BASE + EMMC_RESP0);
            dev->last_r1 = mmio_read(EMMC_BASE + EMMC_RESP1);
            dev->last_r2 = mmio_read(EMMC_BASE + EMMC_RESP2);
            dev->last_r3 = mmio_read(EMMC_BASE + EMMC_RESP3);
            break;
    }

    // If with data, wait for the appropriate interrupt
    if((cmd_reg & SD_CMD_ISDATA) && (is_sdma == 0)) {
        uint32_t wr_irpt;
        int is_write = 0;
        if(cmd_reg & SD_CMD_DAT_DIR_CH)
            wr_irpt = (1 << 5);     // read
        else {
            is_write = 1;
            wr_irpt = (1 << 4);     // write
        }

        int cur_block = 0;
        uint32_t *cur_buf_addr = (uint32_t *)dev->buf;
        while(cur_block < dev->blocks_to_transfer) {
#ifdef EMMC_DEBUG
            if(dev->blocks_to_transfer > 1)
				uart_printf("SD: multi block transfer, awaiting block %i ready\n", cur_block);
#endif
            TIMEOUT_WAIT(mmio_read(EMMC_BASE + EMMC_INTERRUPT) & (wr_irpt | 0x8000), timeout);
            irpts = mmio_read(EMMC_BASE + EMMC_INTERRUPT);
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff0000 | wr_irpt);

            if((irpts & (0xffff0000 | wr_irpt)) != wr_irpt) {
#ifdef EMMC_DEBUG
                uart_printf("SD: error occured whilst waiting for data ready interrupt\n");
#endif
                dev->last_error = irpts & 0xffff0000;
                dev->last_interrupt = irpts;
                return;
            }

            // Transfer the block
            uint64_t cur_byte_no = 0;
            while(cur_byte_no < dev->block_size) {
                if(is_write) {
                    uint32_t data = read_word((uint8_t *)cur_buf_addr, 0);
                    mmio_write(EMMC_BASE + EMMC_DATA, data);
                } else {
                    uint32_t data = mmio_read(EMMC_BASE + EMMC_DATA);
                    write_word(data, (uint8_t *)cur_buf_addr, 0);
                }
                cur_byte_no += 4;
                cur_buf_addr++;
            }
#ifdef EMMC_DEBUG
            uart_printf("SD: block %i transfer complete\n", cur_block);
#endif
            cur_block++;
        }
    }

    // Wait for transfer complete (set if read/write transfer or with busy)
    if((((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) ||
        (cmd_reg & SD_CMD_ISDATA)) && (is_sdma == 0)) {
        // First check command inhibit (DAT) is not already 0
        if((mmio_read(EMMC_BASE + EMMC_STATUS) & 0x2) == 0)
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff0002);
        else {
            TIMEOUT_WAIT(mmio_read(EMMC_BASE + EMMC_INTERRUPT) & 0x8002, timeout);
            irpts = mmio_read(EMMC_BASE + EMMC_INTERRUPT);
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff0002);

            // Handle the case where both data timeout and transfer complete
            //  are set - transfer complete overrides data timeout: HCSS 2.2.17
            if(((irpts & 0xffff0002) != 0x2) && ((irpts & 0xffff0002) != 0x100002)) {
#ifdef EMMC_DEBUG
                uart_printf("SD: error occured whilst waiting for transfer complete interrupt\n");
#endif
                dev->last_error = irpts & 0xffff0000;
                dev->last_interrupt = irpts;
                return;
            }
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff0002);
        }
    } else if (is_sdma) {
        // For SDMA transfers, we have to wait for either transfer complete,
        //  DMA int or an error

        // First check command inhibit (DAT) is not already 0
        if((mmio_read(EMMC_BASE + EMMC_STATUS) & 0x2) == 0)
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff000a);
        else {
            TIMEOUT_WAIT(mmio_read(EMMC_BASE + EMMC_INTERRUPT) & 0x800a, timeout);
            irpts = mmio_read(EMMC_BASE + EMMC_INTERRUPT);
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffff000a);

            // Detect errors
            if((irpts & 0x8000) && ((irpts & 0x2) != 0x2)) {
#ifdef EMMC_DEBUG
                uart_printf("SD: error occured whilst waiting for transfer complete interrupt\n");
#endif
                dev->last_error = irpts & 0xffff0000;
                dev->last_interrupt = irpts;
                return;
            }

            // Detect DMA interrupt without transfer complete
            // Currently not supported - all block sizes should fit in the
            //  buffer
            if((irpts & 0x8) && ((irpts & 0x2) != 0x2)) {
#ifdef EMMC_DEBUG
                uart_printf("SD: error: DMA interrupt occured without transfer complete\n");
#endif
                dev->last_error = irpts & 0xffff0000;
                dev->last_interrupt = irpts;
                return;
            }

            // Detect transfer complete
            if(irpts & 0x2) {
#ifdef EMMC_DEBUG
                uart_printf("SD: SDMA transfer complete");
#endif
                // Transfer the data to the user buffer
                memcpy(dev->buf, (const void *)SDMA_BUFFER, dev->block_size);
            } else {
                // Unknown error
#ifdef EMMC_DEBUG
                if(irpts == 0)
                    uart_printf("SD: timeout waiting for SDMA transfer to complete\n");
                else
                    uart_printf("SD: unknown SDMA transfer error\n");

                uart_printf("SD: INTERRUPT: %08x, STATUS %08x\n", irpts, mmio_read(EMMC_BASE + EMMC_STATUS));
#endif

                if((irpts == 0) && ((mmio_read(EMMC_BASE + EMMC_STATUS) & 0x3) == 0x2)) {
                    // The data transfer is ongoing, we should attempt to stop
                    //  it
#ifdef EMMC_DEBUG
                    uart_printf("SD: aborting transfer\n");
#endif
                    mmio_write(EMMC_BASE + EMMC_CMDTM, sd_commands[STOP_TRANSMISSION]);

#ifdef EMMC_DEBUG
                    // pause to let us read the screen
                    udelay(2000000);
#endif
                }
                dev->last_error = irpts & 0xffff0000;
                dev->last_interrupt = irpts;
                return;
            }
        }
    }

    // Return success
    dev->last_cmd_success = 1;
}

// Handle a card interrupt
static void sd_handle_card_interrupt(struct emmc_block_dev *dev) {
#ifdef EMMC_DEBUG
    uint32_t status = mmio_read(EMMC_BASE + EMMC_STATUS);
    uart_printf("SD: card interrupt\n");
    uart_printf("SD: controller status: %08x\n", status);
#endif

    // Get the card status
    if(dev->card_rca) {
        sd_issue_command_int(dev, sd_commands[SEND_STATUS], dev->card_rca << 16,
                             500000);
#ifdef EMMC_DEBUG
        if(FAIL(dev)) {
            uart_printf("SD: unable to get card status\n");
        } else {
            uart_printf("SD: card status: %08x\n", dev->last_r0);
        }
#endif
    } else {
#ifdef EMMC_DEBUG
        uart_printf("SD: no card currently selected\n");
#endif
    }
}

static void sd_handle_interrupts(struct emmc_block_dev *dev) {
    uint32_t irpts = mmio_read(EMMC_BASE + EMMC_INTERRUPT);
    uint32_t reset_mask = 0;

    if(irpts & SD_COMMAND_COMPLETE) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious command complete interrupt\n");
#endif
        reset_mask |= SD_COMMAND_COMPLETE;
    }

    if(irpts & SD_TRANSFER_COMPLETE) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious transfer complete interrupt\n");
#endif
        reset_mask |= SD_TRANSFER_COMPLETE;
    }

    if(irpts & SD_BLOCK_GAP_EVENT) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious block gap event interrupt\n");
#endif
        reset_mask |= SD_BLOCK_GAP_EVENT;
    }

    if(irpts & SD_DMA_INTERRUPT) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious DMA interrupt\n");
#endif
        reset_mask |= SD_DMA_INTERRUPT;
    }

    if(irpts & SD_BUFFER_WRITE_READY) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious buffer write ready interrupt\n");
#endif
        reset_mask |= SD_BUFFER_WRITE_READY;
        sd_reset_dat();
    }

    if(irpts & SD_BUFFER_READ_READY) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious buffer read ready interrupt\n");
#endif
        reset_mask |= SD_BUFFER_READ_READY;
        sd_reset_dat();
    }

    if(irpts & SD_CARD_INSERTION) {
#ifdef EMMC_DEBUG
        uart_printf("SD: card insertion detected\n");
#endif
        reset_mask |= SD_CARD_INSERTION;
    }

    if(irpts & SD_CARD_REMOVAL) {
#ifdef EMMC_DEBUG
        uart_printf("SD: card removal detected\n");
#endif
        reset_mask |= SD_CARD_REMOVAL;
        dev->card_removal = 1;
    }

    if(irpts & SD_CARD_INTERRUPT) {
#ifdef EMMC_DEBUG
        uart_printf("SD: card interrupt detected\n");
#endif
        sd_handle_card_interrupt(dev);
        reset_mask |= SD_CARD_INTERRUPT;
    }

    if(irpts & 0x8000) {
#ifdef EMMC_DEBUG
        uart_printf("SD: spurious error interrupt: %08x\n", irpts);
#endif
        reset_mask |= 0xffff0000;
    }

    mmio_write(EMMC_BASE + EMMC_INTERRUPT, reset_mask);
}

static void sd_issue_command(struct emmc_block_dev *dev, uint32_t command, uint32_t argument, useconds_t timeout) {
    // First, handle any pending interrupts
    sd_handle_interrupts(dev);

    // Stop the command issue if it was the card remove interrupt that was
    //  handled
    if(dev->card_removal) {
        dev->last_cmd_success = 0;
        return;
    }

    // Now run the appropriate commands by calling sd_issue_command_int()
    if(command & IS_APP_CMD) {
        command &= 0xff;
#ifdef EMMC_DEBUG
        uart_printf("SD: issuing command ACMD%i\n", command);
#endif
        if(sd_acommands[command] == SD_CMD_RESERVED(0)) {
            uart_printf("SD: invalid command ACMD%i\n", command);
            dev->last_cmd_success = 0;
            return;
        }
        dev->last_cmd = APP_CMD;

        uint32_t rca = 0;
        if(dev->card_rca)
            rca = dev->card_rca << 16;
        sd_issue_command_int(dev, sd_commands[APP_CMD], rca, timeout);
        if(dev->last_cmd_success) {
            dev->last_cmd = command | IS_APP_CMD;
            sd_issue_command_int(dev, sd_acommands[command], argument, timeout);
        }
    } else {
#ifdef EMMC_DEBUG
        uart_printf("SD: issuing command CMD%i\n", command);
#endif

        if(sd_commands[command] == SD_CMD_RESERVED(0)) {
            uart_printf("SD: invalid command CMD%i\n", command);
            dev->last_cmd_success = 0;
            return;
        }

        dev->last_cmd = command;
        sd_issue_command_int(dev, sd_commands[command], argument, timeout);
    }

#ifdef EMMC_DEBUG
    if(FAIL(dev)) {
        uart_printf("SD: error issuing command: interrupts %08x: ", dev->last_interrupt);
        if(dev->last_error == 0)
            uart_printf("TIMEOUT");
        else {
            for(int i = 0; i < SD_ERR_RSVD; i++) {
                if(dev->last_error & (1 << (i + 16))) {
                    uart_printf(err_irpts[i]);
                    uart_printf(" ");
                }
            }
        }
		uart_printf("\n");
    } else
        uart_printf("SD: command completed successfully\n");
#endif
}

int sd_card_init(struct block_device **dev) {
    // Check the sanity of the sd_commands and sd_acommands structures
    if(sizeof(sd_commands) != (64 * sizeof(uint32_t))) {
        uart_printf("EMMC: fatal error, sd_commands of incorrect size: %i"
               " expected %i\n", sizeof(sd_commands),
               64 * sizeof(uint32_t));
        return -1;
    }
    if(sizeof(sd_acommands) != (64 * sizeof(uint32_t))) {
        uart_printf("EMMC: fatal error, sd_acommands of incorrect size: %i"
               " expected %i\n", sizeof(sd_acommands),
               64 * sizeof(uint32_t));
        return -1;
    }

    // Power cycle the card to ensure its in its startup state
    if(bcm_2708_power_cycle() != 0) {
        uart_printf("EMMC: BCM2708 controller did not power cycle successfully\n");
    }
#ifdef EMMC_DEBUG
    else
		uart_printf("EMMC: BCM2708 controller power-cycled\n");
#endif

    // Read the controller version
    uint32_t ver = mmio_read(EMMC_BASE + EMMC_SLOTISR_VER);
    uint32_t vendor = ver >> 24;
    uint32_t sdversion = (ver >> 16) & 0xff;
    uint32_t slot_status = ver & 0xff;
    uart_printf("EMMC: vendor %x, sdversion %x, slot_status %x\n", vendor, sdversion, slot_status);
    hci_ver = sdversion;

    if(hci_ver < 2) {
        uart_printf("EMMC: only SDHCI versions >= 3.0 are supported\n");
		return -1;
    }

    // Reset the controller
#ifdef EMMC_DEBUG
    uart_printf("EMMC: resetting controller\n");
#endif
    uint32_t control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
    control1 |= (1 << 24);
    // Disable clock
    control1 &= ~(1 << 2);
    control1 &= ~(1 << 0);
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    TIMEOUT_WAIT((mmio_read(EMMC_BASE + EMMC_CONTROL1) & (0x7 << 24)) == 0, 1000000);
    if((mmio_read(EMMC_BASE + EMMC_CONTROL1) & (0x7 << 24)) != 0)
    {
        uart_printf("EMMC: controller did not reset properly\n");
        return -1;
    }
#ifdef EMMC_DEBUG
    uart_printf("EMMC: control0: %08x, control1: %08x, control2: %08x\n",
			mmio_read(EMMC_BASE + EMMC_CONTROL0),
			mmio_read(EMMC_BASE + EMMC_CONTROL1),
            mmio_read(EMMC_BASE + EMMC_CONTROL2));
#endif

    // Read the capabilities registers
    capabilities_0 = mmio_read(EMMC_BASE + EMMC_CAPABILITIES_0);
    capabilities_1 = mmio_read(EMMC_BASE + EMMC_CAPABILITIES_1);
#ifdef EMMC_DEBUG
    uart_printf("EMMC: capabilities: %08x%08x\n", capabilities_1, capabilities_0);
#endif

    // Check for a valid card
#ifdef EMMC_DEBUG
    uart_printf("EMMC: checking for an inserted card\n");
#endif
    TIMEOUT_WAIT(mmio_read(EMMC_BASE + EMMC_STATUS) & (1 << 16), 500000);
    uint32_t status_reg = mmio_read(EMMC_BASE + EMMC_STATUS);
    if((status_reg & (1 << 16)) == 0) {
        uart_printf("EMMC: no card inserted\n");
        return -1;
    }
#ifdef EMMC_DEBUG
    uart_printf("EMMC: status: %08x\n", status_reg);
#endif

    // Clear control2
    mmio_write(EMMC_BASE + EMMC_CONTROL2, 0);

    // Get the base clock rate
    uint32_t base_clock = sd_get_base_clock_hz();
    if(base_clock == 0) {
        uart_printf("EMMC: assuming clock rate to be 100MHz\n");
        base_clock = 100000000;
    }

    // Set clock rate to something slow
#ifdef EMMC_DEBUG
    uart_printf("EMMC: setting clock rate\n");
#endif
    control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
    control1 |= 1;			// enable clock

    // Set to identification frequency (400 kHz)
    uint32_t f_id = sd_get_clock_divider(base_clock, SD_CLOCK_ID);
    if(f_id == SD_GET_CLOCK_DIVIDER_FAIL) {
        uart_printf("EMMC: unable to get a valid clock divider for ID frequency\n");
        return -1;
    }
    control1 |= f_id;

    control1 |= (7 << 16);		// data timeout = TMCLK * 2^10
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    TIMEOUT_WAIT(mmio_read(EMMC_BASE + EMMC_CONTROL1) & 0x2, 0x1000000);
    if((mmio_read(EMMC_BASE + EMMC_CONTROL1) & 0x2) == 0) {
        uart_printf("EMMC: controller's clock did not stabilise within 1 second\n");
        return -1;
    }
#ifdef EMMC_DEBUG
    uart_printf("EMMC: control0: %08x, control1: %08x\n",
			mmio_read(EMMC_BASE + EMMC_CONTROL0),
			mmio_read(EMMC_BASE + EMMC_CONTROL1));
#endif

    // Enable the SD clock
#ifdef EMMC_DEBUG
    uart_printf("EMMC: enabling SD clock\n");
#endif
    udelay(2000);
    control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
    control1 |= 4;
    mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);
    udelay(2000);
#ifdef EMMC_DEBUG
    uart_printf("EMMC: SD clock enabled\n");
#endif

    // Mask off sending interrupts to the ARM
    mmio_write(EMMC_BASE + EMMC_IRPT_EN, 0);
    // Reset interrupts
    mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffffffff);
    // Have all interrupts sent to the INTERRUPT register
    mmio_write(EMMC_BASE + EMMC_IRPT_MASK, 0xffffffff);

#ifdef EMMC_DEBUG
    uart_printf("EMMC: interrupts disabled\n");
#endif
    udelay(2000);

    // Prepare the device structure
    struct emmc_block_dev *ret;
    if(*dev == 0x0)
        ret = (struct emmc_block_dev *)kmalloc(sizeof(struct emmc_block_dev));
    else
        ret = (struct emmc_block_dev *)*dev;

    if(ret == 0x0) {
        uart_printf("EMMC: Failed preparing device\n");
        return -1;
    }

    memset(ret, 0, sizeof(struct emmc_block_dev));
    ret->bd.driver_name = driver_name;
    ret->bd.device_name = device_name;
    ret->bd.block_size = 512;
    ret->bd.read = sd_read;
    ret->bd.write = sd_write;
    ret->bd.supports_multiple_block_read = 1;
    ret->bd.supports_multiple_block_write = 1;
    ret->base_clock = base_clock;

#ifdef EMMC_DEBUG
    uart_printf("EMMC: device structure created\n");
#endif

    // Send CMD0 to the card (reset to idle state)
    sd_issue_command(ret, GO_IDLE_STATE, 0, 500000);
    if(FAIL(ret)) {
        uart_printf("SD: no CMD0 response\n");
        return -1;
    }

    // Send CMD8 to the card
    // Voltage supplied = 0x1 = 2.7-3.6V (standard)
    // Check pattern = 10101010b (as per PLSS 4.3.13) = 0xAA
#ifdef EMMC_DEBUG
    uart_printf("SD: note a timeout error on the following command (CMD8) is normal "
           "and expected if the SD card version is less than 2.0\n");
#endif
    sd_issue_command(ret, SEND_IF_COND, 0x1aa, 500000);
    int v2_later = 0;
    if(TIMEOUT(ret))
        v2_later = 0;
    else if(CMD_TIMEOUT(ret)) {
        if(sd_reset_cmd() == -1)
            return -1;
        mmio_write(EMMC_BASE + EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        v2_later = 0;
    } else if(FAIL(ret)) {
        uart_printf("SD: failure sending CMD8 (%08x)\n", ret->last_interrupt);
        return -1;
    } else {
        if((ret->last_r0 & 0xfff) != 0x1aa) {
            uart_printf("SD: unusable card\n");
#ifdef EMMC_DEBUG
            uart_printf("SD: CMD8 response %08x\n", ret->last_r0);
#endif
            return -1;
        } else
            v2_later = 1;
    }

    // Here we are supposed to check the response to CMD5 (HCSS 3.6)
    // It only returns if the card is a SDIO card
#ifdef EMMC_DEBUG
    uart_printf("SD: note that a timeout error on the following command (CMD5) is "
           "normal and expected if the card is not a SDIO card.\n");
#endif
    sd_issue_command(ret, IO_SET_OP_COND, 0, 10000);
    if(!TIMEOUT(ret)) {
        if(CMD_TIMEOUT(ret)) {
            if(sd_reset_cmd() == -1)
                return -1;
            mmio_write(EMMC_BASE + EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        } else {
            uart_printf("SD: SDIO card detected - not currently supported\n");
#ifdef EMMC_DEBUG
            uart_printf("SD: CMD5 returned %08x\n", ret->last_r0);
#endif
            return -1;
        }
    }

    // Call an inquiry ACMD41 (voltage window = 0) to get the OCR
#ifdef EMMC_DEBUG
    uart_printf("SD: sending inquiry ACMD41\n");
#endif
    sd_issue_command(ret, ACMD(41), 0, 500000);
    if(FAIL(ret)) {
        uart_printf("SD: inquiry ACMD41 failed\n");
        return -1;
    }
#ifdef EMMC_DEBUG
    uart_printf("SD: inquiry ACMD41 returned %08x\n", ret->last_r0);
#endif

    // Call initialization ACMD41
    int card_is_busy = 1;
    while(card_is_busy) {
        uint32_t v2_flags = 0;
        if(v2_later) {
            // Set SDHC support
            v2_flags |= (1 << 30);
            // Set 1.8v support
            if(!ret->failed_voltage_switch)
                v2_flags |= (1 << 24);
        }

        sd_issue_command(ret, ACMD(41), 0x00ff8000 | v2_flags, 500000);
        if(FAIL(ret)) {
            uart_printf("SD: error issuing ACMD41\n");
            return -1;
        }

        if((ret->last_r0 >> 31) & 0x1) {
            // Initialization is complete
            ret->card_ocr = (ret->last_r0 >> 8) & 0xffff;
            ret->card_supports_sdhc = (ret->last_r0 >> 30) & 0x1;

            if(!ret->failed_voltage_switch)
                ret->card_supports_18v = (ret->last_r0 >> 24) & 0x1;

            card_is_busy = 0;
        } else {
            // Card is still busy
#ifdef EMMC_DEBUG
            uart_printf("SD: card is busy, retrying\n");
#endif
            udelay(500000);
        }
    }

#ifdef EMMC_DEBUG
    uart_printf("SD: card identified: OCR: %04x, 1.8v support: %i, SDHC support: %i\n",
			ret->card_ocr, ret->card_supports_18v, ret->card_supports_sdhc);
#endif

    // At this point, we know the card is definitely an SD card, so will definitely
    //  support SDR12 mode which runs at 25 MHz
    sd_switch_clock_rate(base_clock, SD_CLOCK_NORMAL);

    // A small wait before the voltage switch
    udelay(5000);

    // Switch to 1.8V mode if possible
    if(ret->card_supports_18v) {
#ifdef EMMC_DEBUG
        uart_printf("SD: switching to 1.8V mode\n");
#endif
        // As per HCSS 3.6.1

        // Send VOLTAGE_SWITCH
        sd_issue_command(ret, VOLTAGE_SWITCH, 0, 500000);
        if(FAIL(ret)) {
#ifdef EMMC_DEBUG
            uart_printf("SD: error issuing VOLTAGE_SWITCH\n");
#endif
            ret->failed_voltage_switch = 1;
            sd_power_off();
            return sd_card_init((struct block_device **)&ret);
        }

        // Disable SD clock
        control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
        control1 &= ~(1 << 2);
        mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);

        // Check DAT[3:0]
        status_reg = mmio_read(EMMC_BASE + EMMC_STATUS);
        uint32_t dat30 = (status_reg >> 20) & 0xf;
        if(dat30 != 0) {
#ifdef EMMC_DEBUG
            uart_printf("SD: DAT[3:0] did not settle to 0\n");
#endif
            ret->failed_voltage_switch = 1;
            sd_power_off();
            return sd_card_init((struct block_device **)&ret);
        }

        // Set 1.8V signal enable to 1
        uint32_t control0 = mmio_read(EMMC_BASE + EMMC_CONTROL0);
        control0 |= (1 << 8);
        mmio_write(EMMC_BASE + EMMC_CONTROL0, control0);

        // Wait 5 ms
        udelay(5000);

        // Check the 1.8V signal enable is set
        control0 = mmio_read(EMMC_BASE + EMMC_CONTROL0);
        if(((control0 >> 8) & 0x1) == 0) {
#ifdef EMMC_DEBUG
            uart_printf("SD: controller did not keep 1.8V signal enable high\n");
#endif
            ret->failed_voltage_switch = 1;
            sd_power_off();
            return sd_card_init((struct block_device **)&ret);
        }

        // Re-enable the SD clock
        control1 = mmio_read(EMMC_BASE + EMMC_CONTROL1);
        control1 |= (1 << 2);
        mmio_write(EMMC_BASE + EMMC_CONTROL1, control1);

        // Wait 1 ms
        udelay(10000);

        // Check DAT[3:0]
        status_reg = mmio_read(EMMC_BASE + EMMC_STATUS);
        dat30 = (status_reg >> 20) & 0xf;
        if(dat30 != 0xf) {
#ifdef EMMC_DEBUG
            uart_printf("SD: DAT[3:0] did not settle to 1111b (%01x)\n", dat30);
#endif
            ret->failed_voltage_switch = 1;
            sd_power_off();
            return sd_card_init((struct block_device **)&ret);
        }
#ifdef EMMC_DEBUG
        uart_printf("SD: voltage switch complete\n");
#endif
    }

    // Send CMD2 to get the cards CID
    sd_issue_command(ret, ALL_SEND_CID, 0, 500000);
    if(FAIL(ret)) {
        uart_printf("SD: error sending ALL_SEND_CID\n");
        return -1;
    }
    uint32_t card_cid_0 = ret->last_r0;
    uint32_t card_cid_1 = ret->last_r1;
    uint32_t card_cid_2 = ret->last_r2;
    uint32_t card_cid_3 = ret->last_r3;

#ifdef EMMC_DEBUG
    uart_printf("SD: card CID: %08x%08x%08x%08x\n", card_cid_3, card_cid_2, card_cid_1, card_cid_0);
#endif
    uint32_t *dev_id = (uint32_t *)kmalloc(4 * sizeof(uint32_t));
    dev_id[0] = card_cid_0;
    dev_id[1] = card_cid_1;
    dev_id[2] = card_cid_2;
    dev_id[3] = card_cid_3;
    ret->bd.device_id = (uint8_t *)dev_id;
    ret->bd.dev_id_len = 4 * sizeof(uint32_t);

    // Send CMD3 to enter the data state
    sd_issue_command(ret, SEND_RELATIVE_ADDR, 0, 500000);
    if(FAIL(ret)) {
        uart_printf("SD: error sending SEND_RELATIVE_ADDR\n");
        kfree(ret);
        return -1;
    }

    uint32_t cmd3_resp = ret->last_r0;
#ifdef EMMC_DEBUG
    uart_printf("SD: CMD3 response: %08x\n", cmd3_resp);
#endif

    ret->card_rca = (cmd3_resp >> 16) & 0xffff;
    uint32_t crc_error = (cmd3_resp >> 15) & 0x1;
    uint32_t illegal_cmd = (cmd3_resp >> 14) & 0x1;
    uint32_t error = (cmd3_resp >> 13) & 0x1;
    uint32_t status = (cmd3_resp >> 9) & 0xf;
    uint32_t ready = (cmd3_resp >> 8) & 0x1;

    if(crc_error) {
        uart_printf("SD: CRC error\n");
        kfree(ret);
        kfree(dev_id);
        return -1;
    }

    if(illegal_cmd) {
        uart_printf("SD: illegal command\n");
        kfree(ret);
        kfree(dev_id);
        return -1;
    }

    if(error) {
        uart_printf("SD: generic error\n");
        kfree(ret);
        kfree(dev_id);
        return -1;
    }

    if(!ready) {
        uart_printf("SD: not ready for data\n");
        kfree(ret);
        kfree(dev_id);
        return -1;
    }

#ifdef EMMC_DEBUG
    uart_printf("SD: RCA: %04x\n", ret->card_rca);
#endif
    // Now select the card (toggles it to transfer state)
    sd_issue_command(ret, SELECT_CARD, ret->card_rca << 16, 500000);
    if(FAIL(ret)) {
        uart_printf("SD: error sending CMD7\n");
        kfree(ret);
        return -1;
    }

    uint32_t cmd7_resp = ret->last_r0;
    status = (cmd7_resp >> 9) & 0xf;

    if((status != 3) && (status != 4)) {
        uart_printf("SD: invalid status (%i)\n", status);
        kfree(ret);
        kfree(dev_id);
        return -1;
    }

    // If not an SDHC card, ensure BLOCKLEN is 512 bytes
    if(!ret->card_supports_sdhc) {
        sd_issue_command(ret, SET_BLOCKLEN, 512, 500000);
        if(FAIL(ret)) {
            uart_printf("SD: error sending SET_BLOCKLEN\n");
            kfree(ret);
            return -1;
        }
    }
    ret->block_size = 512;
    uint32_t controller_block_size = mmio_read(EMMC_BASE + EMMC_BLKSIZECNT);
    controller_block_size &= (~0xfff);
    controller_block_size |= 0x200;
    mmio_write(EMMC_BASE + EMMC_BLKSIZECNT, controller_block_size);

    // Get the cards SCR register
    ret->scr = (struct sd_scr *)kmalloc(sizeof(struct sd_scr));
    ret->buf = &ret->scr->scr[0];
    ret->block_size = 8;
    ret->blocks_to_transfer = 1;
    sd_issue_command(ret, SEND_SCR, 0, 500000);
    ret->block_size = 512;
    if(FAIL(ret)) {
        uart_printf("SD: error sending SEND_SCR\n");
        kfree(ret->scr);
        kfree(ret);
        return -1;
    }

    // Determine card version
    // Note that the SCR is big-endian
    uint32_t scr0 = byte_swap(ret->scr->scr[0]);
    ret->scr->sd_version = SD_VER_UNKNOWN;
    uint32_t sd_spec = (scr0 >> (56 - 32)) & 0xf;
    uint32_t sd_spec3 = (scr0 >> (47 - 32)) & 0x1;
    uint32_t sd_spec4 = (scr0 >> (42 - 32)) & 0x1;
    ret->scr->sd_bus_widths = (scr0 >> (48 - 32)) & 0xf;
    if(sd_spec == 0)
        ret->scr->sd_version = SD_VER_1;
    else if(sd_spec == 1)
        ret->scr->sd_version = SD_VER_1_1;
    else if(sd_spec == 2) {
        if(sd_spec3 == 0)
            ret->scr->sd_version = SD_VER_2;
        else if(sd_spec3 == 1) {
            if(sd_spec4 == 0)
                ret->scr->sd_version = SD_VER_3;
            else if(sd_spec4 == 1)
                ret->scr->sd_version = SD_VER_4;
        }
    }

#ifdef EMMC_DEBUG
    uart_printf("SD: &scr: %08x\n", &ret->scr->scr[0]);
    uart_printf("SD: SCR[0]: %08x, SCR[1]: %08x\n", ret->scr->scr[0], ret->scr->scr[1]);;
    uart_printf("SD: SCR: %08x%08x\n", byte_swap(ret->scr->scr[0]), byte_swap(ret->scr->scr[1]));
    uart_printf("SD: SCR: version %s, bus_widths %01x\n", sd_versions[ret->scr->sd_version],
           ret->scr->sd_bus_widths);
#endif

    if(ret->scr->sd_bus_widths & 0x4) {
        // Set 4-bit transfer mode (ACMD6)
        // See HCSS 3.4 for the algorithm
#ifdef EMMC_DEBUG
        uart_printf("SD: switching to 4-bit data mode\n");
#endif

        // Disable card interrupt in host
        uint32_t old_irpt_mask = mmio_read(EMMC_BASE + EMMC_IRPT_MASK);
        uint32_t new_iprt_mask = old_irpt_mask & ~(1 << 8);
        mmio_write(EMMC_BASE + EMMC_IRPT_MASK, new_iprt_mask);

        // Send ACMD6 to change the card's bit mode
        sd_issue_command(ret, SET_BUS_WIDTH, 0x2, 500000);
        if(FAIL(ret))
            uart_printf("SD: switch to 4-bit data mode failed\n");
        else {
            // Change bit mode for Host
            uint32_t control0 = mmio_read(EMMC_BASE + EMMC_CONTROL0);
            control0 |= 0x2;
            mmio_write(EMMC_BASE + EMMC_CONTROL0, control0);

            // Re-enable card interrupt in host
            mmio_write(EMMC_BASE + EMMC_IRPT_MASK, old_irpt_mask);

#ifdef EMMC_DEBUG
            uart_printf("SD: switch to 4-bit complete\n");
#endif
        }
    }

    uart_printf("SD: found a valid version %s SD card\n", sd_versions[ret->scr->sd_version]);
#ifdef EMMC_DEBUG
    uart_printf("SD: setup successful (status %i)\n", status);
#endif

    // Reset interrupt register
    mmio_write(EMMC_BASE + EMMC_INTERRUPT, 0xffffffff);

    *dev = (struct block_device *)ret;

    return 0;
}

static int sd_ensure_data_mode(struct emmc_block_dev *edev)
{
    if(edev->card_rca == 0) {
        // Try again to initialise the card
        int ret = sd_card_init((struct block_device **)&edev);
        if(ret != 0)
            return ret;
    }

#ifdef EMMC_DEBUG
    uart_printf("SD: ensure_data_mode() obtaining status register for card_rca %08x: ",
		edev->card_rca);
#endif

    sd_issue_command(edev, SEND_STATUS, edev->card_rca << 16, 500000);
    if(FAIL(edev)) {
        uart_printf("SD: ensure_data_mode() error sending CMD13\n");
        edev->card_rca = 0;
        return -1;
    }

    uint32_t status = edev->last_r0;
    uint32_t cur_state = (status >> 9) & 0xf;
#ifdef EMMC_DEBUG
    uart_printf("status %i\n", cur_state);
#endif
    if(cur_state == 3) {
        // Currently in the stand-by state - select it
        sd_issue_command(edev, SELECT_CARD, edev->card_rca << 16, 500000);
        if(FAIL(edev)) {
            uart_printf("SD: ensure_data_mode() no response from CMD17\n");
            edev->card_rca = 0;
            return -1;
        }
    } else if(cur_state == 5) {
        // In the data transfer state - cancel the transmission
        sd_issue_command(edev, STOP_TRANSMISSION, 0, 500000);
        if(FAIL(edev)) {
            uart_printf("SD: ensure_data_mode() no response from CMD12\n");
            edev->card_rca = 0;
            return -1;
        }

        // Reset the data circuit
        sd_reset_dat();
    } else if(cur_state != 4) {
        // Not in the transfer state - re-initialise
        int ret = sd_card_init((struct block_device **)&edev);
        if(ret != 0)
            return ret;
    }

    // Check again that we're now in the correct mode
    if(cur_state != 4) {
#ifdef EMMC_DEBUG
        uart_printf("SD: ensure_data_mode() rechecking status: ");
#endif
        sd_issue_command(edev, SEND_STATUS, edev->card_rca << 16, 500000);
        if(FAIL(edev)) {
            uart_printf("SD: ensure_data_mode() no response from CMD13\n");
            edev->card_rca = 0;
            return -1;
        }
        status = edev->last_r0;
        cur_state = (status >> 9) & 0xf;

#ifdef EMMC_DEBUG
        uart_printf("%i\n", cur_state);
#endif

        if(cur_state != 4) {
            uart_printf("SD: unable to initialise SD card to "
                   "data mode (state %i)\n", cur_state);
            edev->card_rca = 0;
            return -1;
        }
    }

    return 0;
}

static int sd_do_data_command(struct emmc_block_dev *edev, int is_write, uint8_t *buf, uint64_t buf_size, uint32_t block_no) {
    // PLSS table 4.20 - SDSC cards use byte addresses rather than block addresses
    if(!edev->card_supports_sdhc)
        block_no *= 512;

    // This is as per HCSS 3.7.2.1
    if(buf_size < edev->block_size) {
        uart_printf("SD: do_data_command() called with buffer size (%i) less than "
               "block size (%i)\n", buf_size, edev->block_size);
        return -1;
    }

    divmod_t mod = divmod(buf_size, edev->block_size);
    edev->blocks_to_transfer = mod.div;
    if(mod.mod) {
        uart_printf("SD: do_data_command() called with buffer size (%i) not an "
               "exact multiple of block size (%i)\n", buf_size, edev->block_size);
        return -1;
    }
    edev->buf = buf;

    // Decide on the command to use
    int command;
    if(is_write) {
        if(edev->blocks_to_transfer > 1)
            command = WRITE_MULTIPLE_BLOCK;
        else
            command = WRITE_BLOCK;
    } else {
        if(edev->blocks_to_transfer > 1)
            command = READ_MULTIPLE_BLOCK;
        else
            command = READ_SINGLE_BLOCK;
    }

    int retry_count = 0;
    int max_retries = 3;
    while(retry_count < max_retries) {
        edev->use_sdma = 0;
        sd_issue_command(edev, command, block_no, 5000000);

        if(SUCCESS(edev))
            break;
        else {
            uart_printf("SD: error sending CMD%i, ", command);
            uart_printf("error = %08x.  ", edev->last_error);
            retry_count++;
            if(retry_count < max_retries)
                uart_printf("Retrying...\n");
            else
                uart_printf("Giving up.\n");
        }
    }
    if(retry_count >= max_retries) {
        edev->card_rca = 0;
        return -1;
    }

    return 0;
}
int sd_read(struct block_device *dev, uint8_t *buf, uint64_t buf_size, uint32_t block_no) {
    // Check the status of the card
    struct emmc_block_dev *edev = (struct emmc_block_dev *)dev;
    if(sd_ensure_data_mode(edev) != 0)
        return -1;

#ifdef EMMC_DEBUG
    uart_printf("SD: read() card ready, reading from block %u\n", block_no);
#endif

    if(sd_do_data_command(edev, 0, buf, buf_size, block_no) < 0)
        return -1;

#ifdef EMMC_DEBUG
    uart_printf("SD: data read successful\n");
#endif

    return buf_size;
}

int sd_write(struct block_device *dev, uint8_t *buf, uint64_t buf_size, uint32_t block_no) {
    // Check the status of the card
    struct emmc_block_dev *edev = (struct emmc_block_dev *)dev;
    if(sd_ensure_data_mode(edev) != 0)
        return -1;

#ifdef EMMC_DEBUG
    uart_printf("SD: write() card ready, reading from block %u\n", block_no);
#endif

    if(sd_do_data_command(edev, 1, buf, buf_size, block_no) < 0)
        return -1;

#ifdef EMMC_DEBUG
    uart_printf("SD: write read successful\n");
#endif

    return buf_size;
}

