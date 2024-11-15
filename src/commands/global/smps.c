#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
#include "ui/ui_term.h"     // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "pirate/amux.h"
#include "hardware/pwm.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"

static const char* const usage[] = {
    "smps\t<v>",
    "Set SMPS to <v> volts",
    "smps\t-s",
    "Show SMPS ADC setpoints (diagnostic)",
};

static const struct ui_help_options options[] = {

    { 0, "-h", T_HELP_FLAG },
};

void smps_handler(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    // SMPS:
    // Output max: 16volts
    // Output protection zener: 18volts (16.8-19.1volts min/max)
    // Feedback divider: 9.1K/3.3K
    // Feedback max output @ 16volts: 4.258volts
    // Feedback protection zener: 4.7volts (4.4-5volts min/max)
    // feedback=((6600*hw_adc_raw[X])/4096);
    // hw_adc_raw[X]=feedback*4096/6600

#define SMPS_R1 9100
#define SMPS_R2 3300
#define VOLTAGE_DIVIDER_TO_ADC (I * R2) / (R1 + R2) = (6600 * ADC) / 4096
// #define SMPS_ADC_SET(X) (4096 * X * SMPS_R2)/(6600*(SMPS_R1+SMPS_R2))
// #define SMPS_ADC_SET (4096 * I * 330)/(6600*(9100+3300))
// #define SMPS_ADC_SET_2(X) (5632/341000)*(X*10000)
#define SMPS_ADC_SET(X) (uint32_t)(((float)((float)X * (float)100.0f) * (float)((float)16516.0f)) / (float)10000.0f)

    float volts;
    bool has_volts = cmdln_args_float_by_position(1, &volts);
    bool has_setpoints = cmdln_args_find_flag('s');

    if (has_setpoints) {
        printf("SMPS ADC SETPOINTS:\r\n");
        for (int i = 5; i < 17; i++) {
            printf("\t%dvolts = %d\r\n", i, SMPS_ADC_SET(i));
        }
    }

    if (((!has_volts) && (!has_setpoints)) || volts < 5.1f || volts > 16.0f) {

        if (volts < 5.1f || volts > 16.0f) {
            printf("Invalid voltage: %1.2f\r\n", volts);
        }

        prompt_result result;
        printf("%sSMPS\r\nVolts (5.1V-16.0V)%s", ui_term_color_info(), ui_term_color_reset());
        ui_prompt_float(&result, 5.1f, 16.0f, 13.0f, true, &volts, false);
        if (result.exit) {
            res->error = true;
            return;
        }
    }

    uint32_t adc_set = SMPS_ADC_SET(volts);
    printf("Setting SMPS to %1.2f volts\r\n", volts);
    printf("ADC SET: %d\r\n", adc_set);
    // X to exit
    printf("Press X to exit\r\n");

    // disable analog subsystem

    // measure voltage
    uint32_t raw = amux_read_bio(BIO3);
#define PWM_TOP 14000 // 0x30D3
    // PWM setup
    // Current adjust is slice 4 channel a
    // voltage adjust is slice 4 channel b
    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[BIO2]);
    uint chan_num = pwm_gpio_to_channel(bio2bufiopin[BIO2]);
    // 10KHz clock, into our 1K + 0.1uF filter
    pwm_set_clkdiv_int_frac(slice_num, 16 >> 4, 16 & 0b1111);
    pwm_set_wrap(slice_num, PWM_TOP);
    pwm_set_chan_level(slice_num, chan_num, (uint16_t)(PWM_TOP / 2));
    bio_output(BIO2);
    // enable output
    gpio_set_function(bio2bufiopin[BIO2], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);
    bool pwm_on = true;

    absolute_time_t start_time = get_absolute_time();

    while (true) {
        // measure voltage
        raw = amux_read_bio(BIO3);
        // adjust PWM
        if (raw > adc_set) {
            // PWM off
            pwm_set_enabled(slice_num, false);
            pwm_on = false;
        } else if (raw < adc_set && !pwm_on) {
            // PWM on
            pwm_set_enabled(slice_num, true);
            pwm_on = true;
        }
        busy_wait_us(500);
        char c;
        if (rx_fifo_try_get(&c)) {
            if (c == 'X' || c == 'x') {
                break;
            }
        }

        int64_t duration_ms = absolute_time_diff_us(start_time, get_absolute_time()) / 1000;
        if (duration_ms > 1000) {
            printf("SMPS voltage: %1.2f\r", (float)((float)raw / 165.0f));
            start_time = get_absolute_time();
        }
    }
    printf("\r\n");
    pwm_set_enabled(slice_num, false);
    bio_input(BIO2);
}
