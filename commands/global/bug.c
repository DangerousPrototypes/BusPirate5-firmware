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

void bug_e9(bool pullup, uint8_t bio_pin) {
    printf("Disabling Bus Pirate pull-ups\r\n");
    pullup_disable();
    printf("Making IO0 buffer and GPIO input\r\n");
    bio_input(bio_pin);
    busy_wait_ms(10);
    printf("Making IO0 buffer an output\r\n");
    bio_buf_output(bio_pin);
    busy_wait_ms(10);
    printf("GPIO pin should be 0: %d\r\n", bio_get(bio_pin));
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
    printf("GPIO pin should be 0: %d\r\n", bio_get(bio_pin));
    if (bio_get(bio_pin)) {
        printf("Warning: GPIO is 1, E9 found\r\n");
    }
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

        if (has_a) {
            for (int i = 0; i < 8; i++) {
                printf("\r\nTest IO%d:\r\n", i);
                bug_e9(true, i);
            }
            return;
        }

        printf("\r\nTest 1:\r\n");
        printf("Pull-down disabled...\r\n");
        gpio_disable_pulls(bio2bufiopin[BIO0]);
        bug_e9(true, BIO0);

        printf("\r\nTest 2:\r\n");
        printf("Set pulls disabled...\r\n");
        gpio_set_pulls(bio2bufiopin[BIO0], false, false);
        bug_e9(true, BIO0);

        printf("\r\nTest 3:\r\n");
        bug_e9(true, BIO0);
        printf("GPIO.IE = false...\r\n");
        gpio_set_input_enabled(bio2bufiopin[BIO0], false);
        busy_wait_ms(10);
        printf("GPIO pin should be 0: %d\r\n", bio_get(BIO0));
        if (bio_get(BIO0)) {
            printf("GPIO is 1, E9 found\r\n");
        }

        printf("\r\nTest 4:\r\n");
        printf("Strong low test...\r\n");
        gpio_set_input_enabled(bio2bufiopin[BIO0], true);
        bug_e9(false, BIO0);
    }
}
