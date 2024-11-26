#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwi2c.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "hwi2c.pio.h"
#include "pirate/hwi2c_pio.h"
#include "pirate/storage.h"
#include "commands/i2c/scan.h"
#include "commands/i2c/demos.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"

static const char pin_labels[][5] = {
    "SDA",
    "SCL",
};

static struct _i2c_mode_config mode_config;

// command configuration
const struct _mode_command_struct hwi2c_commands[] = {
    {   .command="scan", 
        .func=&i2c_search_addr, 
        .description_text=T_HELP_I2C_SCAN, 
        .supress_fala_capture=true
    },
    {   .command="si7021", 
        .func=&demo_si7021, 
        .description_text=T_HELP_I2C_SI7021, 
        .supress_fala_capture=true
    },
    {   .command="ms5611", 
        .func=&demo_ms5611, 
        .description_text=T_HELP_I2C_MS5611, 
        .supress_fala_capture=true
    },
    {   .command="tsl2561", 
        .func=&demo_tsl2561, 
        .description_text=T_HELP_I2C_TSL2561, 
        .supress_fala_capture=true
    },
};
const uint32_t hwi2c_commands_count = count_of(hwi2c_commands);

uint32_t hwi2c_setup(void) {
    uint32_t temp;

    // menu items options
    static const struct prompt_item i2c_clock_stretch_menu[] = { { T_OFF }, {T_ON} };
    static const struct prompt_item i2c_data_bits_menu[] = { { T_HWI2C_DATA_BITS_MENU_1 },
                                                             { T_HWI2C_DATA_BITS_MENU_2 } };
    static const struct prompt_item i2c_speed_menu[] = { { T_HWI2C_SPEED_MENU_1 } };

    static const struct ui_prompt i2c_menu[] = { { .description = T_HWI2C_SPEED_MENU,
                                                   .menu_items = i2c_speed_menu,
                                                   .menu_items_count = count_of(i2c_speed_menu),
                                                   .prompt_text = T_HWI2C_SPEED_PROMPT,
                                                   .minval = 1,
                                                   .maxval = 1000,
                                                   .defval = 400,
                                                   .menu_action = 0,
                                                   .config = &prompt_int_cfg },
                                                    { .description = T_HWI2C_DATA_BITS_MENU,
                                                    .menu_items = i2c_data_bits_menu,
                                                    .menu_items_count = count_of(i2c_data_bits_menu),
                                                    .prompt_text = T_HWI2C_DATA_BITS_PROMPT,
                                                    .minval = 0,
                                                    .maxval = 1,
                                                    .defval = 0,
                                                    .menu_action = 0,
                                                    .config = &prompt_list_cfg },
                                                    { .description = T_HWI2C_CLOCK_STRETCH_MENU,
                                                    .menu_items = i2c_clock_stretch_menu,
                                                    .menu_items_count = count_of(i2c_clock_stretch_menu),
                                                    .prompt_text = T_OFF,
                                                    .minval = 0,
                                                    .maxval = 1,
                                                    .defval = 1,
                                                    .menu_action = 0,
                                                    .config = &prompt_list_cfg }
                                                  };

    const char config_file[] = "bpi2c.bp";

    const mode_config_t config_t[] = {
        // clang-format off
        { "$.baudrate", &mode_config.baudrate, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.data_bits", &mode_config.data_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.clock_stretch", (uint32_t*)&mode_config.clock_stretch, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };
    prompt_result result;

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        hwi2c_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }
    ui_prompt_uint32(&result, &i2c_menu[0], &mode_config.baudrate);
    if (result.exit) {
        return 0;
    }
    ui_prompt_uint32(&result, &i2c_menu[2], &temp);
    if (result.exit) {
        return 0;
    }    
    mode_config.clock_stretch = (bool)(temp-1);
    // printf("Result: %d\r\n", mode_config.baudrate);
    // ui_prompt_uint32(&result, &i2c_menu[1], &temp);
    // if(result.exit) return 0;
    // mode_config.data_bits=(uint8_t)temp-1;
    storage_save_mode(config_file, config_t, count_of(config_t));
    return 1;
}

uint32_t hwi2c_setup_exc(void) {
    pio_i2c_init(bio2bufiopin[M_I2C_SDA],
                bio2bufiopin[M_I2C_SCL],
                bio2bufdirpin[M_I2C_SDA],
                bio2bufdirpin[M_I2C_SCL],
                mode_config.baudrate,
                mode_config.clock_stretch);           
    system_bio_claim(true, M_I2C_SDA, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, M_I2C_SCL, BP_PIN_MODE, pin_labels[1]);
    mode_config.start_sent = false;
    return 1;
}

bool hwi2c_preflight_sanity_check(void) {
    return ui_help_sanity_check(true, 1<<M_I2C_SDA|1<<M_I2C_SCL);
}

bool hwi2c_error(hwi2c_status_t error, struct _bytecode* result) {
    switch (error) {
        case HWI2C_TIMEOUT:
            result->error_message = GET_T(T_HWI2C_TIMEOUT);
            result->error = SERR_ERROR;
            pio_i2c_resume_after_error();
            return true;
            break;
        default:
            return false;
    }
}

void hwi2c_start(struct _bytecode* result, struct _bytecode* next) {

    hwi2c_status_t i2c_status;
    if(!mode_config.start_sent) {
        result->data_message = GET_T(T_HWI2C_START);
        i2c_status = pio_i2c_start_timeout(0xfffff);
    }else{
        result->data_message = GET_T(T_HWI2C_REPEATED_START);
        i2c_status = pio_i2c_restart_timeout(0xfffff);
    }

    if (!hwi2c_error(i2c_status, result)) {
        mode_config.start_sent = true;
    }
}

void hwi2c_stop(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HWI2C_STOP);
    hwi2c_status_t i2c_status = pio_i2c_stop_timeout(0xffff);
    hwi2c_error(i2c_status, result);
    mode_config.start_sent = false;
}

void hwi2c_write(struct _bytecode* result, struct _bytecode* next) {
    hwi2c_status_t i2c_status = pio_i2c_write_timeout(result->out_data, 0xffff);
    hwi2c_error(i2c_status, result);
    result->data_message = (i2c_status != HWI2C_OK ? GET_T(T_HWI2C_NACK) : GET_T(T_HWI2C_ACK));
}

void hwi2c_read(struct _bytecode* result, struct _bytecode* next) {
    // if next is start, stop, startr or stopr, then NACK
    bool ack = true;
    if (next) {
        switch (next->command) {
            case SYN_START_ALT:
            case SYN_STOP_ALT:
            case SYN_START:
            case SYN_STOP:
                ack = false;
                break;
        }
    }
    hwi2c_status_t i2c_status = pio_i2c_read_timeout((uint8_t *)&result->in_data, ack, 0xffff);
    hwi2c_error(i2c_status, result);
    result->data_message = (ack ? GET_T(T_HWI2C_ACK) : GET_T(T_HWI2C_NACK));
}

void hwi2c_macro(uint32_t macro) {
    uint32_t result = 0;
    switch (macro) {
        case 0:
            printf(" 0. Macro menu\r\n");
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}

void hwi2c_cleanup(void) {
    pio_i2c_cleanup();
    bio_init();
    system_bio_claim(false, M_I2C_SDA, BP_PIN_MODE, 0);
    system_bio_claim(false, M_I2C_SCL, BP_PIN_MODE, 0);
}

void hwi2c_settings(void) {
    //printf(" %s: %dkHz\r\n", GET_T(T_HWI2C_SPEED_MENU), mode_config.baudrate);
    ui_prompt_mode_settings_int(GET_T(T_HWI2C_SPEED_MENU), mode_config.baudrate, GET_T(T_KHZ));
    // printf(" %s: %s\r\n", GET_T(T_HWI2C_DATA_BITS_MENU), GET_T(i2c_data_bits_menu[mode_config.data_bits].description));
    //printf(" %s: %s\r\n", GET_T(T_HWI2C_CLOCK_STRETCH_MENU),
    //        (mode_config.clock_stretch ? GET_T(T_ON) : GET_T(T_OFF)));
    ui_prompt_mode_settings_string(GET_T(T_HWI2C_CLOCK_STRETCH_MENU),
            (mode_config.clock_stretch ? GET_T(T_ON) : GET_T(T_OFF)), 0x00);
}

void hwi2c_help(void) {
    printf("Muli-Master-multi-slave 2 wire protocol using a CLOCK and a bidirectional DATA\r\n");
    printf("line in opendrain configuration. Standard clock frequencies are 100kHz, 400kHz\r\n");
    printf("and 1MHz.\r\n");
    printf("\r\n");
    printf("More info: https://en.wikipedia.org/wiki/I2C\r\n");
    printf("\r\n");
    printf("Electrical:\r\n");
    printf("\r\n");
    printf("BPCMD\t   { |            ADDRESS(7bits+R/!W bit)             |\r\n");
    printf("CMD\tSTART| A6  | A5  | A4  | A3  | A2  | A1  | A0  | R/!W| ACK* \r\n");
    printf("\t-----|-----|-----|-----|-----|-----|-----|-----|-----|-----\r\n");
    printf("SDA\t\"\"___|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_ ..\r\n");
    printf("SCL\t\"\"\"\"\"|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__ ..\r\n");
    printf("\r\n");
    printf("BPCMD\t   |                      DATA (8bit)              |     |  ]  |\r\n");
    printf("CMD\t.. | D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | ACK*| STOP|  \r\n");
    printf("\t  -|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|\r\n");
    printf("SDA\t.. |_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|___\"\"|\r\n");
    printf("SCL\t.. |__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|\"\"\"\"\"|\r\n");
    printf("\r\n");
    printf("* Receiver needs to pull SDA down when address/byte is received correctly\r\n");
    printf("\r\n");
    printf("Connection:\r\n");
    printf("\t\t  +--[2k]---+--- +3V3 or +5V0\r\n");
    printf("\t\t  | +-[2k]--|\r\n");
    printf("\t\t  | |\r\n");
    printf("\tSDA \t--+-|------------- SDA\r\n");
    printf("{BP}\tSCL\t----+------------- SCL  {DUT}\r\n");
    printf("\tGND\t------------------ GND\r\n\r\n");

    ui_help_mode_commands(hwi2c_commands, hwi2c_commands_count);
}

uint32_t hwi2c_get_speed(void) {
    return mode_config.baudrate;
}
