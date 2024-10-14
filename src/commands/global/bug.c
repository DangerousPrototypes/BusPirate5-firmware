#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "ui/ui_cmdln.h"
#include "pirate/pullup.h"
#include "pirate/bio.h"
#include "pirate/psu.h"
#include "hardware/pio.h"
#include "pwm.pio.h"

static const char* const usage[] = { "replicate hardware bugs", "Test errata E9: bug e9" };

static const struct ui_help_options options[] = {
    { 0, "-h", T_HELP_FLAG },
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

bool bug_e9_seems_fixed(bool pullup, uint8_t bio_pin) {
    bool bug_seems_fixed = false;
    printf("Disabling Bus Pirate pull-ups\r\n");
    pullup_disable();
    printf("Making IO0 buffer and GPIO input\r\n");
    bio_input(bio_pin);
    busy_wait_ms(10);
    printf("Making IO0 buffer an output\r\n");
    bio_buf_output(bio_pin);
    busy_wait_ms(10);

    bool pin_state = bio_get(bio_pin);
    printf("GPIO pin should be 0: %d\r\n", bio_get(bio_pin));
    if (pin_state) {
        printf("Warning: GPIO is 1, cannot test for E9\r\n");
    } else {
        printf("Making IO0 buffer and GPIO input\r\n");
        bio_input(bio_pin);
        if (pullup) {
            printf("Enabling Bus Pirate pull-ups\r\n");
            pullup_enable();
        }
        printf("Making IO0 buffer an output\r\n");
        bio_buf_output(bio_pin);
        printf("Disabling Bus Pirate pull-ups\r\n");
        pullup_disable();
        busy_wait_ms(10);

        pin_state = bio_get(bio_pin);
        printf("GPIO pin should be 0: %d\r\n", pin_state);
        if (pin_state) {
            printf("Warning: GPIO is 1, E9 found\r\n");
        } else {
            bug_seems_fixed = true;
        }
    }
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
    if (ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options))) {
        return;
    }

    char bug_str[4];
    bool verb_e9 = false;
    if (cmdln_args_string_by_position(1, sizeof(bug_str), bug_str)) {
        if (strcmp(bug_str, "e9") == 0) {
            verb_e9 = true;
        } else if (strcmp(bug_str, "qe9") == 0) {
            e9_qualify();
        }
    }

    // look for a flag
    bool has_a = cmdln_args_find_flag('a');

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
                bool current_pin_fixed = bug_e9_seems_fixed(true, i);
                e9_seems_fixed = e9_seems_fixed && current_pin_fixed;
            }
        } else {
            bool tmp;

            printf("\r\nTest 1:\r\n");
            printf("Pull-down disabled...\r\n");
            gpio_disable_pulls(bio2bufiopin[BIO0]);
            tmp = bug_e9_seems_fixed(true, BIO0);
            e9_seems_fixed = e9_seems_fixed && tmp;

            printf("\r\nTest 2:\r\n");
            printf("Set pulls disabled...\r\n");
            gpio_set_pulls(bio2bufiopin[BIO0], false, false);
            tmp = bug_e9_seems_fixed(true, BIO0);
            e9_seems_fixed = e9_seems_fixed && tmp;

            printf("\r\nTest 3:\r\n");
            tmp = bug_e9_seems_fixed(true, BIO0);
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
            tmp = bug_e9_seems_fixed(false, BIO0);
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
