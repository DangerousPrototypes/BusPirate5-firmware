/**
 * @file v_adc.c
 * @brief ADC voltage measurement command implementation.
 * @details Implements the v/V commands for measuring voltages on I/O pins:
 *          - v: Single voltage measurement
 *          - V: Continuous voltage measurement with averaging
 *          
 *          Command syntax:
 *          - v [pin]: Measure voltage on pin (or all pins if omitted)
 *          - V [pin]: Continuous measurement on pin (or all pins)
 *          
 *          Measurement specifications:
 *          - Resolution: 12-bit ADC (0-4096 counts)
 *          - Range: 0 to 6.6V (2:1 resistor divider)
 *          - Accuracy: Depends on reference voltage calibration
 *          - Averaging: Exponential moving average for V command
 *          
 *          Pin voltage routing:
 *          - Uses CD4067 analog multiplexer
 *          - Automatically sweeps all ADC channels
 *          - Results cached for display
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"
#include "command_struct.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_info.h"
#include "system_config.h"
#include "commands/global/freq.h"
#include "usb_rx.h"
#include "pirate/amux.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"

static const char* const usage[] = {
    "v/V [io]",
    "Measure pin 0 voltage:%s v 0",
    "Continuous measurement pin 0:%s V 0",
    "Measure voltage on all pins:%s v",
    "Continuous measurement on all pins:%s V",
};

static const bp_command_positional_t adc_positionals[] = {
    { "io", "pin", T_HELP_GCMD_ADC_PIN, false },
    { 0 }
};

const bp_command_def_t adc_single_def = {
    .name         = "v",
    .description  = T_CMDLN_ADC_ONE,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals      = adc_positionals,
    .positional_count = 1,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

const bp_command_def_t adc_cont_def = {
    .name         = "V",
    .description  = T_CMDLN_ADC_CONT,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals      = adc_positionals,
    .positional_count = 1,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void adc_measure(struct command_result* res, bool refresh);

uint32_t adc_print(uint8_t bio_pin, bool refresh) {
    // sweep adc
    amux_sweep();
    printf("%s%s IO%d:%s %s%d.%03d%sV\r%s",
           ui_term_color_info(),
           GET_T(T_MODE_ADC_VOLTAGE),
           bio_pin,
           ui_term_color_reset(),
           ui_term_color_num_float(),
           ((*hw_pin_voltage_ordered[bio_pin + 1]) / 1000),
           ((*hw_pin_voltage_ordered[bio_pin + 1]) % 1000),
           ui_term_color_reset(),
           (refresh ? "" : "\n"));
    return 1;
}

uint32_t adc_print_average(uint8_t bio_pin, bool refresh) {
    // sweep adc
    amux_sweep();
    printf("%s%s IO%d:%s %s%d.%03d%sV (Average: %s%d.%03d%sV)\r%s",
           ui_term_color_info(),
           GET_T(T_MODE_ADC_VOLTAGE),
           bio_pin,
           ui_term_color_reset(),
           ui_term_color_num_float(),
           ((*hw_pin_voltage_ordered[bio_pin + 1]) / 1000),
           ((*hw_pin_voltage_ordered[bio_pin + 1]) % 1000),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           (get_adc_average(*hw_pin_avgsum_voltage_ordered[bio_pin + 1]) / 1000),
           (get_adc_average(*hw_pin_avgsum_voltage_ordered[bio_pin + 1]) % 1000),
           ui_term_color_reset(),
           (refresh ? "" : "\n"));
    return 1;
}

void adc_measure_single(struct command_result* res) {
    adc_measure(res, false);
}

void adc_measure_cont(struct command_result* res) {
    adc_measure(res, true);
}

void adc_measure(struct command_result* res, bool refresh) {
    const bp_command_def_t *def = refresh ? &adc_cont_def : &adc_single_def;
    if (bp_cmd_help_check(def, res->help_flag)) {
        return;
    }
    uint32_t temp;
    bool has_value = bp_cmd_get_positional_uint32(def, 1, &temp);

    if (!has_value) { // show voltage on all pins
        if (refresh) {
            // TODO: use the ui_prompt_continue function, but how to deal with the names and labels????
            printf("%s%s%s\r\n%s",
                   ui_term_color_notice(),
                   GET_T(T_PRESS_ANY_KEY_TO_EXIT),
                   ui_term_color_reset(),
                   ui_term_cursor_hide());
            system_config.terminal_hide_cursor = true;
        }

        ui_info_print_pin_names();
        ui_info_print_pin_labels();

        if (refresh) {
            char c;
            do {
                ui_info_print_pin_voltage(true);
                busy_wait_ms(250);
            } while (!rx_fifo_try_get(&c));
            system_config.terminal_hide_cursor = false;
            printf("%s", ui_term_cursor_show()); // show cursor
        }
        // for single measurement, also adds final \n for continuous
        ui_info_print_pin_voltage(false);
        return;
    }

    // pin bounds check
    if (temp >= count_of(bio2bufiopin)) {
        printf("Error: Pin IO%d is invalid", temp);
        res->error = true;
        return;
    }

    if (refresh) {
        // continuous measurement on this pin
        //  press any key to continue
        prompt_result result;
        reset_adc_average = true;
        system_config.terminal_hide_cursor = true;
        printf("%s", ui_term_cursor_hide());
        ui_prompt_any_key_continue(&result, 250, &adc_print_average, temp, true);
        printf("%s", ui_term_cursor_show());
        system_config.terminal_hide_cursor = false;
    }
    // single measurement, also adds final \n for cont mode
    adc_print(temp, false);
}
