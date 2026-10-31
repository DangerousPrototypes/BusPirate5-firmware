#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hw3wire.h"
#include "pirate/bio.h"
#include "ui/ui_cmdln.h"
#include "hw2wire.pio.h"
#include "pirate/hw3wire_pio.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "ui/ui_format.h"
#include "ui/ui_help.h"
#include "commands/2wire/sle4442.h"
#include "lib/bp_args/bp_cmd.h"

static const char pin_labels[][5] = { "MOSI", "SCLK", "MISO", "CS" };
struct _hw3wire_mode_config mode_config;
static uint8_t checkshort(void);

// command configuration
const struct _mode_command_struct hw3wire_commands[] = { 0 };
const uint32_t hw3wire_commands_count = count_of(hw3wire_commands);

// Speed — flag -s / --speed (kHz; stored as Hz)
static const bp_val_constraint_t hw3wire_speed_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 3900, .def = 100 },
    .prompt = T_SPEED,
    .hint = T_HW3WIRE_SPEED_MENU_1,
};

// CS idle — flag -c / --csidle
static const bp_val_choice_t hw3wire_csidle_choices[] = {
    { "low",  NULL, T_HWSPI_CS_IDLE_MENU_1, 0 },
    { "high", NULL, T_HWSPI_CS_IDLE_MENU_2, 1 },
};
static const bp_val_constraint_t hw3wire_csidle_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = hw3wire_csidle_choices, .count = 2, .def = 1 },
    .prompt = T_HWSPI_CS_IDLE_MENU,
};

static const bp_command_opt_t hw3wire_setup_opts[] = {
    { "speed",  's', BP_ARG_REQUIRED, "1-3900",   0, &hw3wire_speed_range },
    { "csidle", 'c', BP_ARG_REQUIRED, "low|high", 0, &hw3wire_csidle_choice },
    { 0 },
};

const bp_command_def_t hw3wire_setup_def = {
    .name = "3wire",
    .description = 0,
    .opts = hw3wire_setup_opts,
};

uint32_t hw3wire_setup(void) {
    uint32_t temp;

    const char config_file[] = "bp3wire.bp";

    const mode_config_t config_t[] = {
        // clang-format off
        { "$.baudrate", &mode_config.baudrate, MODE_CONFIG_FORMAT_DECIMAL },
        /*{ "$.data_bits", &mode_config.data_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.stop_bits", &mode_config.clock_polarity, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.parity", &mode_config.clock_phase, MODE_CONFIG_FORMAT_DECIMAL },*/
        { "$.cs_idle", &mode_config.cs_idle, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    // Detect interactive vs CLI mode by checking the primary flag
    bp_cmd_status_t st = bp_cmd_flag(&hw3wire_setup_def, 's', &temp);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            hw3wire_settings();
            int r = bp_cmd_yes_no_exit("");
            if (r == BP_YN_EXIT) return 0; // exit
            if (r == BP_YN_YES)  return 1; // use saved settings
        }

        if (bp_cmd_prompt(&hw3wire_speed_range, &temp) != BP_CMD_OK) return 0;
        mode_config.baudrate = temp * 1000;

        if (bp_cmd_prompt(&hw3wire_csidle_choice, &temp) != BP_CMD_OK) return 0;
        mode_config.cs_idle = (uint8_t)temp;
    } else {
        // Speed already parsed — multiply by 1000 to convert kHz → Hz
        mode_config.baudrate = temp * 1000;

        st = bp_cmd_flag(&hw3wire_setup_def, 'c', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.cs_idle = (uint8_t)temp;
    }

    storage_save_mode(config_file, config_t, count_of(config_t));

    return 1;
}

void hw3wire_set_cs(uint8_t cs) {
    if (cs == M_3WIRE_SELECT) { // 'start'
        if (mode_config.cs_idle) {
            bio_put(M_3WIRE_CS, 0); //select
        } else {
            bio_put(M_3WIRE_CS, 1); //deselect
        }
    } else { // 'stop'
        if (mode_config.cs_idle) {
            bio_put(M_3WIRE_CS, 1); //deselect
        } else {
            bio_put(M_3WIRE_CS, 0); //select
        }
    }
}

uint32_t hw3wire_setup_exc(void) {
    //buffers to correct position
    bio_buf_output(M_3WIRE_MOSI);
    bio_buf_output(M_3WIRE_SCLK);
    bio_buf_input(M_3WIRE_MISO);   
    bio_set_function(M_SPI_CS, GPIO_FUNC_SIO);
    bio_output(M_SPI_CS); 
    pio_hw3wire_init(bio2bufiopin[M_3WIRE_MOSI],
                     bio2bufiopin[M_3WIRE_SCLK],
                     bio2bufiopin[M_3WIRE_MISO],                     
                     mode_config.baudrate);
    system_bio_update_purpose_and_label(true, M_3WIRE_MOSI, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_3WIRE_SCLK, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, M_3WIRE_MISO, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, M_3WIRE_CS, BP_PIN_MODE, pin_labels[3]);
    //pio_hw3wire_reset();
    //bio_put(M_2WIRE_RST, 0); // preload the RST pin to be 0 when output
    hw3wire_set_cs(M_3WIRE_DESELECT);
    return 1;
}

void hw3wire_cleanup(void) {
    pio_hw3wire_cleanup();
    bio_init();
    system_bio_update_purpose_and_label(false, M_3WIRE_MOSI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_3WIRE_SCLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_3WIRE_MISO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_3WIRE_CS, BP_PIN_MODE, 0);
}

bool hw3wire_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 0);
}

void hw3wire_start(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = false;
    result->data_message = GET_T(T_HWSPI_CS_SELECT);
    hw3wire_set_cs(M_3WIRE_SELECT);
}

void hw3wire_start_alt(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = true;
    result->data_message = GET_T(T_HWSPI_CS_SELECT);
    hw3wire_set_cs(M_3WIRE_SELECT);
}

void hw3wire_stop(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = false;
    result->data_message = GET_T(T_HWSPI_CS_DESELECT);
    hw3wire_set_cs(M_3WIRE_DESELECT);
}

void hw3wire_stop_alt(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = false;
    result->data_message = GET_T(T_HWSPI_CS_DESELECT);
    hw3wire_set_cs(M_3WIRE_DESELECT);
}

void hw3wire_write(struct _bytecode* result, struct _bytecode* next) {
    uint32_t temp = result->out_data;
    ui_format_bitorder_manual(&temp, result->bits, system_config.bit_order);
    //pio_hw3wire_put16(temp);
    pio_hw3wire_get16((uint8_t*)&temp);
    result->in_data = temp; //hwspi_write_read((uint8_t)result->out_data);
    result->read_with_write = mode_config.read_with_write;    
}

void hw3wire_read(struct _bytecode* result, struct _bytecode* next) {
    uint8_t temp8=0xff;
    //pio_hw3wire_get16(&temp8);
    pio_hw3wire_get16(&temp8);
    uint32_t temp = temp8;
    //ui_format_bitorder_manual(&temp, result->bits, system_config.bit_order);
    result->in_data = temp;
}

void hw3wire_tick_clock(struct _bytecode* result, struct _bytecode* next) {
    pio_hw3wire_clock_tick();
}

void hw3wire_set_clk_high(struct _bytecode* result, struct _bytecode* next) {
    pio_hw3wire_set_mask(0b10, 0b10);
    result->out_data = 1;
}

void hw3wire_set_clk_low(struct _bytecode* result, struct _bytecode* next) {
    pio_hw3wire_set_mask(0b10, 0);
    result->out_data = 0;
}

void hw3wire_set_dat_high(struct _bytecode* result, struct _bytecode* next) {
    pio_hw3wire_set_mask(0b01, 0b01);
    result->out_data = 1;
}

void hw3wire_set_dat_low(struct _bytecode* result, struct _bytecode* next) {
    pio_hw3wire_set_mask(0b01, 0b00);
    result->out_data = 0;
}

void hw3wire_read_bit(struct _bytecode* result, struct _bytecode* next) {
    result->in_data = bio_get(M_3WIRE_MISO);
}

void hw3wire_macro(uint32_t macro) {
    uint32_t result = 0;
    switch (macro) {
        case 0:
            printf(" 0. Macro menu\r\n");
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }

    if (result) {
        printf("Device not found\r\n");
    }
}

void hw3wire_settings(void) {
    ui_help_setting_int(GET_T(T_SPEED), mode_config.baudrate/1000, GET_T(T_KHZ));
    ui_help_setting_string(GET_T(T_HWSPI_CS_IDLE_MENU), GET_T(hw3wire_csidle_choices[mode_config.cs_idle].label), 0x00);
}

void hw3wire_help(void) {
    printf("%s3 wire SPI-like bus with individual pin control (/\\_-^.)\r\n", ui_term_color_info());
    printf("Pins: data out, clock, data in, and CS\r\n");
    printf("[ & ] Toggle CS low and HIGH\r\n");
    ui_help_mode_commands(hw3wire_commands, hw3wire_commands_count);
/*printf("\r\nAvailable mode commands:\r\n");
for(uint32_t i=0;i<hw3wire_commands_count;i++)
{
    printf("%s%s%s\t%s%s\r\n", ui_term_color_prompt(), hw3wire_commands[i].command,
        ui_term_color_info(), hw3wire_commands[i].help_text?t[hw3wire_commands[i].help_text]:"Unavailable",
ui_term_color_reset());
}*/
#if 0
	printf("\r\n");
	printf("More info: https://en.wikipedia.org/wiki/I2C\r\n");
	printf("\r\n");
	printf("Electrical:\r\n");
	printf("\r\n");
	printf("BPCMD\t   { |            ADDRES(7bits+R/!W bit)             |\r\n");
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
	printf("\tGND\t------------------ GND\r\n");
#endif
}

uint32_t hw3wire_get_speed(void) {
    return mode_config.baudrate;
}

//-----------------------------------------
//
// Flatbuffer/binary access functions
//-----------------------------------------

bool bpio_hw3wire_configure(bpio_mode_configuration_t *bpio_mode_config){
    if(bpio_mode_config->debug) printf("[3WIRE] Speed %d Hz\r\n", bpio_mode_config->speed);
    mode_config.baudrate = bpio_mode_config->speed;
    mode_config.cs_idle = bpio_mode_config->chip_select_idle;
    return true;  
}
