/**
 * @file p_pullups.c
 * @brief Programmable pullup/pulldown resistor command implementation.
 * @details Implements the p/P commands for controlling pullup/pulldown resistors:
 *          - p: Disable all pullups/pulldowns
 *          - P: Enable pullups with configuration
 *          
 *          Command syntax (BP5 Rev10+ with I2C resistor network):
 *          - P [value] [-d] [-p <pins>]
 *          - Values: OFF, 1.3K, 1.5K, 1.8K, 2.2K, 3.2K, 4.7K, 10K, 1M
 *          - -d: Pull down instead of pull up
 *          - -p: Specify pins (e.g., -p 0123 for pins 0-3)
 *          
 *          Hardware implementation:
 *          - Uses two MCP23017 I2C I/O expanders
 *          - Four resistors per pin (2.2K, 4.7K, 10K, 1M)
 *          - Values achieved by parallel combination
 *          - Direction controlled separately (pullup vs pulldown)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pirate/amux.h"
#include "pirate/pullup.h"
#include "lib/bp_args/bp_cmd.h"

#if BP_HW_PULLX
const char* const p_usage[] = {
    "P [resistor value] [-d] [-p <pins>]",
    "Quick enable 10K pull-up resistors on all pins: P",
    "Enable 2.2K pull-up on all pins: P 2.2k",
    "Enable 1M pull-down on all pins: P 1M -d",
    "Enable 10K pull-up on pins 0 and 1: P 10k -p 01",  
    "Disable: p",
};

static const bp_command_opt_t pullx_opts[] = {
    { "down", 'd', BP_ARG_NONE, NULL, 0, NULL },
    { "pins", 'p', BP_ARG_REQUIRED, "01234567", 0, NULL },
    { 0 }
};

static const bp_command_positional_t pullx_positionals[] = {
    { "value", NULL, 0, false, NULL },
    { 0 }
};

const bp_command_def_t pullups_enable_def = {
    .name = "P",
    .description = T_HELP_GCMD_P,
    .opts = pullx_opts,
    .positionals = pullx_positionals,
    .positional_count = 1,
    .usage = p_usage,
    .usage_count = count_of(p_usage),
};

const bp_command_def_t pullups_disable_def = {
    .name = "p",
    .description = T_HELP_GCMD_P,
    .usage = p_usage,
    .usage_count = count_of(p_usage),
};
#else
const char* const p_usage[] = {
    "p|P",
    "Disable: p",
    "Enable: P",
};

const bp_command_def_t pullups_enable_def = {
    .name = "P",
    .description = T_HELP_GCMD_P,
    .usage = p_usage,
    .usage_count = count_of(p_usage),
};

const bp_command_def_t pullups_disable_def = {
    .name = "p",
    .description = T_HELP_GCMD_P,
    .usage = p_usage,
    .usage_count = count_of(p_usage),
};
#endif

#if BP_HW_PULLX
    void pullx_show_settings(void){
        //display the current configuration
        for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
            printf("|  IO%d\t", i);
        }
        printf("|\r\n");
        for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
            uint8_t pull;
            bool pull_up;
            pullx_get_pin(i, &pull, &pull_up);  

            if(pull == PULLX_OFF) {
                printf("|%s\t", pullx_options[pull].name);
            }else{
                printf("|%s %s\t", pullx_options[pull].name, pull ? "U" : "D");
            }
        }
        printf("|\r\n");
    }

    // get command line arguments, false = fail, true = success
    bool pullx_parse_args(uint8_t *pullx, uint8_t *pin_args, bool *direction){
        //search for trailing arguments
        char action_str[9]; // somewhere to store the parameter string
        bool r_found = false; // default pullup value (10K)
        if (bp_cmd_get_positional_string(&pullups_enable_def, 1, action_str, sizeof(action_str))) {
            strupr(action_str);
            for(uint8_t i=0; i<count_of(pullx_options); i++) {
                if (strcmp(action_str, pullx_options[i].name) == 0) {
                    (*pullx) = pullx_options[i].pull;
                    r_found = true;
                    break;
                }
            }
            if(!r_found) {
                printf("Invalid resistor value, valid values are:\r\n");
                for(uint8_t i=0; i<count_of(pullx_options); i++) {
                    printf("%s ", pullx_options[i].name);
                }
                return false;
            }
        }else{
            (*pullx) = PULLX_10K; //default to 10K
        }

        //pull up is the default
        if(bp_cmd_find_flag(&pullups_enable_def, 'd')) {
            (*direction) = 0;
        }else{
            (*direction) = 1;
        }

        (*pin_args)=0;
        if(bp_cmd_get_string(&pullups_enable_def, 'p', action_str, sizeof(action_str))) {
            for(uint8_t i=0; i<sizeof(action_str); i++) {
                if(action_str[i]==0) break;
                if(action_str[i] < '0' || action_str[i] > '7') {
                    printf("Invalid pin number: %c\r\n", action_str[i]);
                    return false;
                }
                //set bit in pin_args
                (*pin_args) |= 1<< (action_str[i] - '0');
            }
        }else{
            (*pin_args) = 0xff;
        }
        return true;
    }
#endif

void pullups_enable(void) {
    system_config.pullup_enabled = 1;
    system_config.info_bar_changed = true;
    pullup_enable();
}

void pullups_enable_handler(struct command_result* res) {
    if (bp_cmd_help_check(&pullups_enable_def, res->help_flag)) {
        #if BP_HW_PULLX
            pullx_show_settings();
        #endif
        return;
    }

    #if BP_HW_PULLX
        uint8_t pullx;
        uint8_t pin_args;
        bool direction;

        if(!pullx_parse_args(&pullx, &pin_args, &direction)) {
            pullx_show_settings(); 
            return;
        }

        // show the configuration
        printf("Pull resistor: %s", pullx_options[pullx].name);
        if(pullx != PULLX_OFF) {
            printf(", Direction: %s", direction ? "UP" : "DOWN");
        }

        printf(", Pins: ");
        if(pin_args == 0xff) {
            printf("All");
        }else{
            for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
                if(pin_args & (1<<i)) {
                    printf("%d ", i);
                }
            }  
        }  
        printf("\r\n");

        //apply the settings and update the pullx configuration
        for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
            if(pin_args & (1<<i)) {
                pullx_set_pin(i, pullx, direction);
            }
        }

        //apply the settings
        pullx_update();
        //show the settings
        pullx_show_settings();
        amux_sweep();
        printf("\r\n\r\n");
    #else
        pullups_enable();
        amux_sweep();
        printf("%s%s:%s %s (%s @ %s%d.%d%sV)\r\n",
            ui_term_color_notice(),
            GET_T(T_MODE_PULLUP_RESISTORS),
            ui_term_color_reset(),
            GET_T(T_MODE_ENABLED),
            BP_HARDWARE_PULLUP_VALUE,
            ui_term_color_num_float(),
            hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] / 1000,
            (hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] % 1000) / 100,
            ui_term_color_reset());
    #endif
    if (hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 250){
        printf(
            "\r\nWarning: no/low voltage detected.\r\nEnable power supply (W) or attach external supply to Vout/Vref");
    }
}

void pullups_disable(void) {   
    system_config.pullup_enabled = 0;
    system_config.info_bar_changed = true;
    #if BP_HW_PULLX
        //1M pull-down when disabled
        pullx_set_all_update(PULLX_1M, false);
    #else
        pullup_disable();
    #endif
}

void pullups_disable_handler(struct command_result* res) {
    if (bp_cmd_help_check(&pullups_disable_def, res->help_flag)) {
        return;
    }

    pullups_disable();

    printf("%s%s:%s %s",
           ui_term_color_notice(),
           GET_T(T_MODE_PULLUP_RESISTORS),
           ui_term_color_reset(),
           GET_T(T_MODE_DISABLED));
  
    #if BP_HW_PULLX
        //show settings
        pullx_show_settings();    
    #endif
}

void pullups_init(void) {
    pullup_init();
}
