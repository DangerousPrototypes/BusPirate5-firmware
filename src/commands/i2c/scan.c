#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "pirate/hwi2c_pio.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hwi2c.h"
#include "lib/i2c_address_list/dev_i2c_addresses.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"

static const char* const usage[] = {
    "scan\t[-v(erbose)] [-h(elp)]",
    "Scan I2C address space: scan",
    "Scan, list possible part numbers: scan -v",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_I2C_SCAN },           // command help
    { 0, "-v", T_HELP_I2C_SCAN_VERBOSE }, // verbose
    { 0, "-h", T_HELP_FLAG },             // help
};

bool i2c_search_check_addr(uint8_t address);

void i2c_search_addr(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    bool verbose = cmdln_args_find_flag('v');
    bool color = false;
    uint16_t device_count = 0;
    uint16_t device_pairs = 0;

    if (hwi2c_checkshort()) {
        ui_help_error(T_HWI2C_NO_PULLUP_DETECTED);
        // printf("No pull-up or short circuit. Enable power (W) and pull-up resistors (P)\r\n");
        system_config.error = 1;
        res->error = true;
        return;
    }
    printf("I2C address search:\r\n");
    for (uint16_t i = 0; i < 256; i = i + 2) {
        bool i2c_w = i2c_search_check_addr(i);
        bool i2c_r = i2c_search_check_addr(i + 1);

        if (i2c_w || i2c_r) {
            device_count += (i2c_w + i2c_r); // add any new devices
            if (i2c_w && i2c_r) {
                device_pairs++;
            }

            color = !color;
            if (color || verbose) {
                ui_term_color_text_background(hw_pin_label_ordered_color[7][0], hw_pin_label_ordered_color[7][1]);
            }

            printf("0x%02X", i >> 1);
            if (i2c_w) {
                printf(" (0x%02X W)", i);
            }
            if (i2c_r) {
                printf(" (0x%02X R)", i + 1);
            }
            if (color || verbose) {
                printf("%s", ui_term_color_reset());
            }
            printf("\r\n");
            if (verbose) {
                printf("%s\r\n", dev_i2c_addresses[i >> 1]);
            }
        }
    }

    printf("%s\r\nFound %d addresses, %d W/R pairs.\r\n", ui_term_color_reset(), device_count, device_pairs);
}

bool i2c_search_check_addr(uint8_t address) {
    bool ack = false;

    hwi2c_status_t i2c_status = pio_i2c_start_timeout(0xfff);
    if (i2c_status == HWI2C_TIMEOUT) {
        pio_i2c_resume_after_error();
    }
    i2c_status = pio_i2c_write_timeout(address, 0xfff);

    switch(i2c_status){
        case HWI2C_OK:
            ack = true;
            break;
        case HWI2C_TIMEOUT:
            pio_i2c_resume_after_error();
            break;
    }

    // if read address then read one and NACK
    if ((ack) && (address & 0x1)) {
        uint32_t temp;
        i2c_status = pio_i2c_read_timeout(&temp, false, 0xfff);
        if (i2c_status==HWI2C_TIMEOUT) {
            pio_i2c_resume_after_error();
        }
    }

    i2c_status = pio_i2c_stop_timeout(0xfff);
    if (i2c_status==HWI2C_TIMEOUT) {
        pio_i2c_resume_after_error();
    }

    return ack;
}
