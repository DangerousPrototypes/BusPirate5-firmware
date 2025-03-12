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

#if BP_HW_PULLX
const char* const p_usage[] = {
    "P [resistor value] [-d] [-p <pins>]",
    "Quick enable 10K pull-up resistors on all pins: P",
    "Enable 2.2K pull-up on all pins: P 2.2k",
    "Enable 1M pull-down on all pins: P 1M -d",
    "Enable 10K pull-up on pins 0 and 1: P 10k -p 01",  
    "Disable: p",
};

const struct ui_help_options p_options[] = {
    { 1, "", T_HELP_GCMD_P }, // command help
    { 0, "p", T_CONFIG_DISABLE },
    { 0, "P", T_CONFIG_ENABLE },
};
#else
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
#endif

#if BP_HW_PULLX
    void pullx_show_settings(void){
        //display the current configuration
        printf("\r\nPull resistor configuration:\r\n");
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
                printf("|%s %s\t", pullx_options[pull].name, pull_up ? "U" : "D");
            }
        }
        printf("|\r\n");
    }

    void pullx_show_values(void){
        printf("\r\nValid resistor values are:\r\n");
        for(uint8_t i=0; i<count_of(pullx_options); i++) {
            printf("%s ", pullx_options[i].name);
        }
        printf("\r\n");
    }

    // get command line arguments, false = fail, true = success
    bool pullx_parse_args(uint8_t *pullx, uint8_t *pin_args, bool *direction){
        //search for trailing arguments
        char action_str[9]; // somewhere to store the parameter string
        bool r_found = false; // default pullup value (10K)
        if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)) {
            strupr(action_str);
            for(uint8_t i=0; i<count_of(pullx_options); i++) {
                if (strcmp(action_str, pullx_options[i].name) == 0) {
                    (*pullx) = pullx_options[i].pull;
                    r_found = true;
                    break;
                }
            }
            if(!r_found) {
                printf("Error: Invalid resistor value\r\n");
                pullx_show_values();
                return false;
            }
        }else{
            (*pullx) = PULLX_10K; //default to 10K
        }

        //pull up is the default
        if(cmdln_args_find_flag('d')) {
            (*direction) = 0;
        }else{
            (*direction) = 1;
        }

        command_var_t arg;
        (*pin_args)=0;
        if(cmdln_args_find_flag_string('p',&arg, sizeof(action_str), action_str)) {
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
    if (ui_help_show(res->help_flag, p_usage, count_of(p_usage), &p_options[0], count_of(p_options))) {
        #if BP_HW_PULLX
            pullx_show_values();
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
        if(!pullx_update()) {
            printf("%sError:%s %sVOUT voltage too low to enable pull-ups%s\r\n", ui_term_color_error(), ui_term_color_info(), ui_term_color_num_float(), ui_term_color_reset());
            //printf("Settings will be applied when VOUT voltage is sufficient\r\n");
        }
        //show the settings
        pullx_show_settings();
        amux_sweep();
    #else
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
    #endif
    if (hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 250){
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
  
    #if BP_HW_PULLX
        //show settings
        printf("\r\n");
        pullx_show_settings();    
    #endif
}

void pullups_init(void) {
    pullup_init();
}
