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

#if 0
void pullx_set_pin(uint8_t pin, uint8_t value, bool direction){
    static uint32_t in_out_mask;    
    static uint32_t up_down_mask;
    
    if(pin>7) return;

    for(uint8_t i=0; i<4; i++){
        //clear current bits
        in_out_mask &= ~(1<< (pin +(i*8)));
        up_down_mask &= ~(1<< (pin +(i*8)));
        
        //check bits in value and set in resistor mask
        if(value & (1<<i)){
            in_out_mask |= 1<< (pin+(i*8)); //make output
            //if the resistor is enabled, set the direction
            if(direction) up_down_mask |= 1<< (pin+(i*8));
        }
    }

}
#endif

enum {
    PULL_OFF=0,
    PULL_1K3,
    PULL_1K5,
    PULL_1K8,
    PULL_2K2,
    PULL_3K2,
    PULL_4K7,
    PULL_10K,
    PULL_1M,
};

struct pullx_options_t {
    uint8_t pull;
    const char name[5];
    uint8_t resistors; // Use a uint8_t to store the boolean values as bitfields
};

#define R2K2_MASK 0x01
#define R4K7_MASK 0x02
#define R10K_MASK 0x08
#define R1M_MASK  0x04

#define SET_RESISTORS(r2k2, r4k7, r10k, r1m) \
    ((r2k2 ? R2K2_MASK : 0) | (r4k7 ? R4K7_MASK : 0) | (r10k ? R10K_MASK : 0) | (r1m ? R1M_MASK : 0))

const struct pullx_options_t pullx_options[] = {
    { .pull=PULL_OFF, .name="OFF", .resistors=SET_RESISTORS(false, false, false, false) },
    { .pull=PULL_1K3, .name="1.3K", .resistors=SET_RESISTORS(true, true, true, false) },
    { .pull=PULL_1K5, .name="1.5K", .resistors=SET_RESISTORS(true, true, false, false) },
    { .pull=PULL_1K8, .name="1.8K", .resistors=SET_RESISTORS(true, false, true, false) },
    { .pull=PULL_2K2, .name="2.2K", .resistors=SET_RESISTORS(true, false, false, false) },
    { .pull=PULL_3K2, .name="3.2K", .resistors=SET_RESISTORS(false, true, true, false) },
    { .pull=PULL_4K7, .name="4.7K", .resistors=SET_RESISTORS(false, true, false, false) },
    { .pull=PULL_10K, .name="10K", .resistors=SET_RESISTORS(false, false, true, false) },
    { .pull=PULL_1M, .name="1M", .resistors=SET_RESISTORS(false, false, false, true) },
};

void pullx_print_bin(uint16_t value){
    for(uint8_t i=0; i<16; i++){
        printf("%d", (value & (1<<i)) ? 1 : 0);
    }
}

bool pullx_update(void){
    uint16_t output_port_register[2]={0,0};
    uint16_t configuration_register[2]={0xffff,0xffff};

    for(uint8_t i=0; i<BIO_MAX_PINS; i++){
        //build the commands to send to the I2C IO expander
        for(uint8_t b=0; b<4; b++){
            //if bit is set in the resistor mask, set the resistor pin to output
            //resistor mask is 1 if enabled
            // however the I2C expander has inverted logic, so clear the bit to enable the resistor
            if(pullx_options[system_config.pullx_value[i]].resistors & (1<<b)){
                uint8_t idx = 0;
                if(i>=4) idx = 1;
                uint16_t pin_mask = (1<<((i-((BIO_MAX_PINS/2)*idx)) + ( (3-b) *(BIO_MAX_PINS/2))));                
                configuration_register[idx] &= ~(pin_mask); //pin to output                    
                if(system_config.pullx_direction & (1<<i)){
                    output_port_register[idx] |= pin_mask; //pin high
                }
            }
        }

    }

    printf("Configuration:");
    pullx_print_bin(configuration_register[0]);
    printf(" ");
    pullx_print_bin(configuration_register[1]);
    printf("\r\nOutput:");
    pullx_print_bin(output_port_register[0]);
    printf(" ");
    pullx_print_bin(output_port_register[1]);
    printf("\r\n");
}

void pullx_set_pin(uint8_t pin, uint8_t pull, bool pull_up){
    system_config.pullx_value[pin] = pull;
    if(pull_up){
        system_config.pullx_direction |= 1<<pin;
    }else{
        system_config.pullx_direction &= ~(1<<pin);
    }
}

void pullups_enable_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, p_usage, count_of(p_usage), &p_options[0], count_of(p_options))) {
        return;
    }

    //search for trailing arguments
    char action_str[9]; // somewhere to store the parameter string
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
            printf("Invalid resistor value, valid values are:\r\n");
            for(uint8_t i=0; i<count_of(pullx_options); i++) {
                printf("%s ", pullx_options[i].name);
            }
            return;
        }
    }else{
        pullx = PULL_10K; //default to 10K
    }

    //pull up is the default
    if(cmdln_args_find_flag('d')) {
        direction = 0;
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
    }else{
        pin_args = 0xff;
    }

    // show the configuration
    printf("Pull resistor: %s", pullx_options[pullx].name);
    if(pullx != PULL_OFF) {
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

    //display the current configuration
    for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
        printf("|  IO%d\t", i);
    }
    printf("|\r\n");
    for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
        if(system_config.pullx_value[i] == PULL_OFF) {
            printf("|%s \t", pullx_options[system_config.pullx_value[i]].name);
        }else{
            printf("|%s %s\t", pullx_options[system_config.pullx_value[i]].name, system_config.pullx_direction & (1<<i) ? "U" : "D");
        }
    }
    printf("|\r\n");

    //temp, reset all pins
    for(uint8_t i=0; i<BIO_MAX_PINS; i++) {
        pullx_set_pin(i, PULL_OFF, false);
    }

    return;



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
#if 0
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
#endif
