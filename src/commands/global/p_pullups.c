#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pirate/amux.h"
#include "pirate/pullup.h"

const char* const p_usage[] = {
    "p|P",
    "Disable: p",
    "Enable: P",
};

const struct ui_help_options p_options[] = {
    { 1, "", T_HELP_GCMD_P }, // command help
    { 0, "p", T_CONFIG_DISABLE },
    { 0, "P", T_CONFIG_ENABLE },
};

void pullups_enable(void) {
    system_config.pullup_enabled = 1;
    system_config.info_bar_changed = true;
    pullup_enable();
}

void pullups_enable_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, p_usage, count_of(p_usage), &p_options[0], count_of(p_options))) {
        return;
    }

    enum {
        PULL_2K2=0,
        PULL_4K7,
        PULL_10K,
        PULL_1M,
        PULL_OFF,
    };

    struct pullx_options_t {
        uint8_t pull;
        const char name[5];
    };

    const struct pullx_options_t pullx_options[] = {
        { PULL_2K2, "2.2K" },
        { PULL_4K7, "4.7K" },
        { PULL_10K, "10K" },
        { PULL_1M, "1M" },
        { PULL_OFF, "OFF" },
    };

    //search for trailing arguments
    char action_str[9];              // somewhere to store the parameter string
    int8_t pullx = -1; // default pullup value (10K)
    bool direction = 1; // default direction value (up)

    if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)) {
        strupr(action_str);
        for(uint8_t i=0; i<count_of(pullx_options); i++) {
            if (strcmp(action_str, pullx_options[i].name) == 0) {
                pullx = pullx_options[i].pull;
                break;
            }
        }
        if(pullx == -1) {
            printf("Invalid resistor value:\r\n Valid values are 2.2K, 4.7K, 10K, 1M, OFF\r\n");
            return;
        }
    }

    command_var_t arg;
    uint8_t pin_args=0;
    if(cmdln_args_find_flag_string('p',&arg, sizeof(action_str), action_str)) {
        for(uint8_t i=0; i<sizeof(action_str); i++) {
            if(action_str[i]==0) break;
            if(action_str[i] < '0' || action_str[i] > '7') {
                printf("Invalid pin number: %c\r\n", action_str[i]);
                return;
            }
            //set bit in pin_args
            pin_args |= 1<< (action_str[i] - '0');
        }
        printf("Pins: ");
        for(uint8_t i=0; i<8; i++) {
            if(pin_args & (1<<i)) {
                printf("%d ", i);
            }
        }
    }

    //pull up is the default
    if(cmdln_args_find_flag('d')) {
        direction = 0;
    }

    if(pullx == -1) {
        pullx = PULL_10K;
    }

    // show the configuration
    printf("Pull resistor: %s", pullx_options[pullx].name);
    if(pullx != PULL_OFF) {
        printf(", Direction: %s", direction ? "UP" : "DOWN");
    }

    pullups_enable();

    amux_sweep();
    printf("\r\n\r\n%s%s:%s %s (%s @ %s%d.%d%sV)",
           ui_term_color_notice(),
           GET_T(T_MODE_PULLUP_RESISTORS),
           ui_term_color_reset(),
           GET_T(T_MODE_ENABLED),
           BP_HARDWARE_PULLUP_VALUE,
           ui_term_color_num_float(),
           hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] / 1000,
           (hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] % 1000) / 100,
           ui_term_color_reset());

    // TODO test outside debug mode
    if (hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 250) // arbitrary
    {
        printf(
            "\r\nWarning: no/low voltage detected.\r\nEnable power supply (W) or attach external supply to Vout/Vref");
    }
}

void pullups_disable(void) {
    system_config.pullup_enabled = 0;
    system_config.info_bar_changed = true;
    pullup_disable();
}

void pullups_disable_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, p_usage, count_of(p_usage), &p_options[0], count_of(p_options))) {
        return;
    }

    pullups_disable();

    printf("%s%s:%s %s",
           ui_term_color_notice(),
           GET_T(T_MODE_PULLUP_RESISTORS),
           ui_term_color_reset(),
           GET_T(T_MODE_DISABLED));
}

void pullups_init(void) {
    pullup_init();
}

const char* const pullx_usage[] = {
    "p|P",
    "Disable: p",
    "Enable: P",
};

const struct ui_help_options pullx_options[] = {
    { 1, "", T_HELP_GCMD_P }, // command help
    //{ 0, "p", T_CONFIG_DISABLE },
    { 0, "h", T_HELP_HELP },
};

void pullx_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, pullx_usage, count_of(pullx_usage), &pullx_options[0], count_of(pullx_options))) {
        return;
    }
}
