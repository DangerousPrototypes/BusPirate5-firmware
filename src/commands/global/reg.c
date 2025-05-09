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
#include "hardware/i2c.h"

static const char* const p_usage[] = {
    "p|P",
    "Disable: p",
    "Enable: P",
};

static const struct ui_help_options p_options[] = {
    { 1, "", T_HELP_GCMD_P }, // command help
    { 0, "p", T_CONFIG_DISABLE },
    { 0, "P", T_CONFIG_ENABLE },
};


//return true for success, false for failure
bool reg_read_i2c(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len) {  
    uint8_t data_out[2];
    data_out[0] = reg;
    i2c_busy_wait(true); 
    bool result = i2c_write_blocking(BP_I2C_PORT, addr, data_out, 1, false) == PICO_ERROR_GENERIC ? false : true;
    if(result) {
        result = i2c_read_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC ? false : true;
    }
    i2c_busy_wait(false);
    return result;
}

static void reg_print_bin(uint8_t *value){
    for(uint8_t j=0; j<2; j++){
        for(uint8_t i=0; i<8; i++){
            printf("%d", (value[j] & (1<<i)) ? 1 : 0);
        }
        printf(" ");
    }
}

void reg_print_reg(uint8_t addr, uint8_t reg){
    uint8_t data[2];
    printf("REG: %02x\t| ", reg);
    if(!reg_read_i2c(addr, reg, data, 2)) {
        printf("I2C read error\r\n");
        return;
    }
    //printf("%02x %02x | ", data[0], data[1]); 
    reg_print_bin(data);
    printf("\r\n");
}

void reg_i2c_reg(uint8_t addr){
    printf("IN ");
    reg_print_reg(addr, 0x00); //input register
    printf("OUT ");
    reg_print_reg(addr, 0x02); //output register
    printf("DIR ");
    reg_print_reg(addr, 0x06); //configuration register
}

void reg_i2c_dump(void) {
    uint8_t data[2];
    printf("I2C Dump:\r\nPullx 4:7 (0x20):\r\n");
    reg_i2c_reg(0x20);
    printf("Pullx 0:3 (0x21):\r\n");
    reg_i2c_reg(0x21);
    printf("IO EXP (0x22):\r\n");
    reg_i2c_reg(0x22);
    printf("\r\nINT pin: %d\r\n", gpio_get(BP_I2C_INTERRUPT));
}

struct reg_options_t {
    const char name[5];
    void (*handler)(void);
};

const struct reg_options_t reg_options[] = {
    {"i2c", reg_i2c_dump},
};

void reg_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, p_usage, count_of(p_usage), &p_options[0], count_of(p_options))) {
        return;
    }

    //search for trailing arguments
    char action_str[9]; // somewhere to store the parameter string
    if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)){
        for(uint8_t i=0; i<count_of(reg_options); i++) {
            if (strcmp(action_str, reg_options[i].name) == 0) {
                reg_options[i].handler();   
                break;
            }
        }
    }
}
