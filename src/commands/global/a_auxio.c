#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "opt_args.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_help.h"

static const char labels[][5] = { "AUXL", "AUXH" };

static const char* const usage[] = {
    "a/A/@ <io> [-h(elp)]",
    "Pin 0 ouput, low: a 0",
    "Pin 2 output, high: A 2",
    "Pin 5 input, read value: @ 5",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_AUXIO }, // command help
    { 0, "a", T_HELP_AUXIO_LOW },   { 0, "A", T_HELP_AUXIO_HIGH },
    { 0, "@", T_HELP_AUXIO_INPUT }, { 0, "<io>", T_HELP_AUXIO_IO },
};

// TODO: binary format puts to all available pins
void auxio(struct command_result* res, bool output, bool level) {
    // check help
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    uint32_t pin;
    bool has_value = cmdln_args_uint32_by_position(1, &pin);
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
        system_bio_claim(true, pin, BP_PIN_IO, labels[level]);
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
        system_bio_claim(false, pin, BP_PIN_IO, 0);
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
