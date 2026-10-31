/**
 * @file a_auxio.c
 * @brief Auxiliary I/O control commands implementation.
 * @details Implements direct pin control commands:
 *          - a <pin>: Set pin as output low
 *          - A <pin>: Set pin as output high
 *          - @ <pin>: Set pin as input and read current value
 *          
 *          Features:
 *          - Validates pin availability (not in use by mode)
 *          - Updates pin labels to show aux status
 *          - Preserves aux pins across mode changes
 */

#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"

static const char labels[][5] = { "AUXL", "AUXH" };

static const char* const usage[] = {
    "a/A/@ <io> [-h(elp)]",
    "Pin 0 ouput, low:%s a 0",
    "Pin 2 output, high:%s A 2",
    "Pin 5 input, read value:%s @ 5",
};

static const bp_command_positional_t auxio_positionals[] = {
    { "io", "pin", T_HELP_GCMD_AUXIO_PIN, true },
    { 0 }
};

const bp_command_def_t auxio_low_def = {
    .name = "a",
    .description = T_CMDLN_AUX_LOW,
    .positionals      = auxio_positionals,
    .positional_count = 1,
    .usage = usage,
    .usage_count = count_of(usage)
};

const bp_command_def_t auxio_high_def = {
    .name = "A",
    .description = T_CMDLN_AUX_HIGH,
    .positionals      = auxio_positionals,
    .positional_count = 1,
    .usage = usage,
    .usage_count = count_of(usage)
};

const bp_command_def_t auxio_input_def = {
    .name = "@",
    .description = T_CMDLN_AUX_IN,
    .positionals      = auxio_positionals,
    .positional_count = 1,
    .usage = usage,
    .usage_count = count_of(usage)
};

// TODO: binary format puts to all available pins
void auxio(struct command_result* res, bool output, bool level) {
    const bp_command_def_t *def = output ? (level ? &auxio_high_def : &auxio_low_def) : &auxio_input_def;
    
    // check help
    if (bp_cmd_help_check(def, res->help_flag)) {
        return;
    }

    uint32_t pin;
    bool has_value = bp_cmd_get_positional_uint32(def, 1, &pin);
    if (!has_value) {
        printf("%sError:%s specify an IO pin (a 1, A 5, @ 0)", ui_term_color_error(), ui_term_color_reset());
        res->error = true;
        return;
    }

    // first make sure the pin is present and available
    if (pin >= count_of(bio2bufiopin)) {
        printf("%sError:%s pin IO%d is invalid", ui_term_color_error(), ui_term_color_reset(), pin);
        res->error = true;
        return;
    }

    // pin is in use for any purposes, except as existing aux pin
    if (system_config.pin_labels[pin + 1] != 0 && !(system_config.aux_active & (0x01 << ((uint8_t)pin)))) {
        printf("%sError:%s IO%d is in use by %s",
               ui_term_color_error(),
               ui_term_color_reset(),
               pin,
               system_config.pin_labels[pin + 1]);
        res->error = true;
        return;
    }

    // output
    if (output) {
        bio_output(pin);
        bio_put(pin, level);
        printf("IO%s%d%s set to%s OUTPUT: %s%d%s\r\n",
               ui_term_color_num_float(),
               pin,
               ui_term_color_notice(),
               ui_term_color_reset(),
               ui_term_color_num_float(),
               level,
               ui_term_color_reset());
        system_bio_update_purpose_and_label(true, pin, BP_PIN_IO, labels[level]);
        system_set_active(true, pin, &system_config.aux_active);
    } else { // input
        bio_input(pin);
        printf("IO%s%d%s set to%s INPUT: %s%d%s\r\n",
               ui_term_color_num_float(),
               pin,
               ui_term_color_notice(),
               ui_term_color_reset(),
               ui_term_color_num_float(),
               bio_get(pin),
               ui_term_color_reset());
        system_bio_update_purpose_and_label(false, pin, BP_PIN_IO, 0);
        system_set_active(false, pin, &system_config.aux_active);
    }
    return;
}

void auxio_high_handler(struct command_result* res) {
    auxio(res, true, 1);
}
void auxio_low_handler(struct command_result* res) {
    auxio(res, true, 0);
}
void auxio_input_handler(struct command_result* res) {
    auxio(res, false, 0);
}
