#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "ui/ui_cmdln.h"
#include "pirate/pullup.h"
#include "pirate/bio.h"
#include "pirate/psu.h"
#include "hardware/pio.h"
#include "pwm.pio.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const usage[] = { 
    "bug\t<bug> [-a]", 
    "Test errata E9:%s bug e9" 
};

static const bp_command_positional_t bug_positionals[] = {
    { "bug", "e9", T_HELP_GCMD_BUG_ID, false },
    { 0 }
};

static const bp_command_opt_t bug_opts[] = {
    { "all", 'a', BP_ARG_NONE, NULL, "Test all IOs for E9" },
    { 0 }
};

const struct bp_command_def bug_def = {
    .name = "bug",
    .description = 0x00,
    .positionals      = bug_positionals,
    .positional_count = 1,
    .usage = usage,
    .usage_count = count_of(usage),
    .opts = bug_opts,
};

// Write `period` to the input shift register
void pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {
    pio_sm_put_blocking(pio, sm, level);
}

bool bug_e9_seems_fixed(bool pullup, uint8_t bio_pin, bool verbose) {
    bool bug_seems_fixed = false;
    if(verbose) printf("Disabling Bus Pirate pull-ups\r\n");
    pullup_disable();
    if(verbose) printf("Making IO%d buffer and GPIO input\r\n", bio_pin);
    bio_input(bio_pin);
    busy_wait_ms(10);
    if(verbose) printf("Making IO%d buffer an output\r\n", bio_pin);
    bio_buf_output(bio_pin);
    busy_wait_ms(10);

    bool pin_state = bio_get(bio_pin);
    if(verbose) printf("GPIO pin should be 0: %d\r\n", bio_get(bio_pin));
    if (pin_state) {
        printf("Warning: GPIO %d is 1, cannot test for E9\r\n", bio_pin);
    } else {
        if(verbose) printf("Making IO%d buffer and GPIO input\r\n", bio_pin);
        bio_input(bio_pin);
        if (pullup) {
            if(verbose) printf("Enabling Bus Pirate pull-ups\r\n");
            pullup_enable();
        }
        if(verbose) printf("Making IO%d buffer an output\r\n", bio_pin);
        bio_buf_output(bio_pin);
        if(verbose) printf("Disabling Bus Pirate pull-ups\r\n");
        pullup_disable();
        busy_wait_ms(10);

        pin_state = bio_get(bio_pin);
        if(verbose) printf("GPIO pin should be 0: %d\r\n", pin_state);
        if (pin_state) {
            if(verbose) printf("Warning: GPIO is 1, E9 found for IO%d\r\n", bio_pin);
        } else {
            bug_seems_fixed = true;
        }
    }
    bio_input(bio_pin);
    return bug_seems_fixed;
}

void e9_qualify(void) {
    // todo get free sm
    PIO pio = pio0;
    int sm = 3;
    uint offset = pio_add_program(pio, &pwm_program);
    printf("Loaded program at %d\n", offset);

    bio_buf_output(BIO3);
    bio_buf_output(BIO4);
    pwm_program_init(pio, sm, offset, bio2bufiopin[BIO3]);
    // pio_pwm_set_period(pio, sm, (1u << 16) - 1);
    pio_pwm_set_period(pio, sm, (1u << 2) - 1);

    int level = 0;
    while (true) {
        printf("Level = %d\n", level);
        pio_pwm_set_level(pio, sm, level * level);
        level = (level + 1) % 256;
        sleep_ms(10);
    }
}

void bug_handler(struct command_result* res) {
    if (bp_cmd_help_check(&bug_def, res->help_flag)) {
        return;
    }

    char bug_str[4];
    bool verb_e9 = false;
    if (bp_cmd_get_positional_string(&bug_def, 1, bug_str, sizeof(bug_str))) {
        if (strcmp(bug_str, "e9") == 0) {
            verb_e9 = true;
        } else if (strcmp(bug_str, "qe9") == 0) {
            e9_qualify();
        }
    }

    // look for a flag
    bool has_a = bp_cmd_find_flag(&bug_def, 'a');

    if (verb_e9) {

        printf("Replicate bug E9\r\n");
        uint32_t vout, isense, vreg;
        bool fuse;
        psu_measure(&vout, &isense, &vreg, &fuse);
        if (vout < 800) {
            printf("Enable a power supply with the 'W' command.\r\n");
            return;
        }

        bool e9_seems_fixed = true;
        if (has_a) {
            for (int i = 0; i < 8; i++) {
                printf("\r\nTest IO%d:\r\n", i);
                bool current_pin_fixed = bug_e9_seems_fixed(true, i, true);
                e9_seems_fixed = e9_seems_fixed && current_pin_fixed;
            }
        } else {
            bool tmp;

            printf("\r\nTest 1:\r\n");
            printf("Pull-down disabled...\r\n");
            gpio_disable_pulls(bio2bufiopin[BIO0]);
            tmp = bug_e9_seems_fixed(true, BIO0, true);
            e9_seems_fixed = e9_seems_fixed && tmp;

            printf("\r\nTest 2:\r\n");
            printf("Set pulls disabled...\r\n");
            gpio_set_pulls(bio2bufiopin[BIO0], false, false);
            tmp = bug_e9_seems_fixed(true, BIO0, true);
            e9_seems_fixed = e9_seems_fixed && tmp;

            printf("\r\nTest 3:\r\n");
            tmp = bug_e9_seems_fixed(true, BIO0, true);
            e9_seems_fixed = e9_seems_fixed && tmp;
            printf("GPIO.IE = false...\r\n");
            gpio_set_input_enabled(bio2bufiopin[BIO0], false);
            busy_wait_ms(10);
            printf("GPIO pin should be 0: %d\r\n", bio_get(BIO0));
            if (bio_get(BIO0)) {
                printf("GPIO is 1, E9 found\r\n");
                e9_seems_fixed = false;
            }

            printf("\r\nTest 4:\r\n");
            printf("Strong low test...\r\n");
            gpio_set_input_enabled(bio2bufiopin[BIO0], true);
            tmp = bug_e9_seems_fixed(false, BIO0, true);
            e9_seems_fixed = e9_seems_fixed && tmp;
        }

        if (e9_seems_fixed) {
            printf("\r\nSummary: E9 was not found\r\n");
        } else if (has_a) {
            printf("\r\nSummary: E9 was found on at least one pin\r\n");
        } else {
            printf("\r\nSummary: E9 was found\r\n");
        }
    }
}
