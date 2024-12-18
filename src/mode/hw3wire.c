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
#include "ui/ui_prompt.h"
#include "ui/ui_cmdln.h"
#include "hw2wire.pio.h"
#include "pirate/hw3wire_pio.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "ui/ui_format.h"
#include "ui/ui_help.h"
#include "commands/2wire/sle4442.h"

static const char pin_labels[][5] = { "MOSI", "SCLK", "MISO", "CS" };
struct _hw3wire_mode_config mode_config;
static uint8_t checkshort(void);

// command configuration
const struct _mode_command_struct hw3wire_commands[] = {
    /*{   .command="sle4442", 
        .func=&sle4442, 
        .description_text=T_HELP_SLE4442, 
        .supress_fala_capture=true
    },*/
};
const uint32_t hw3wire_commands_count = count_of(hw3wire_commands);


static const struct prompt_item hw3wire_speed_menu[] = { { T_HW3WIRE_SPEED_MENU_1 } };
static const struct prompt_item hw3wire_bits_menu[] = { { T_HWSPI_BITS_MENU_1 } };
static const struct prompt_item hw3wire_polarity_menu[] = { { T_HWSPI_CLOCK_POLARITY_MENU_1 },
                                                        { T_HWSPI_CLOCK_POLARITY_MENU_2 } };
static const struct prompt_item hw3wire_phase_menu[] = { { T_HWSPI_CLOCK_PHASE_MENU_1 }, { T_HWSPI_CLOCK_PHASE_MENU_2 } };
static const struct prompt_item hw3wire_idle_menu[] = { { T_HWSPI_CS_IDLE_MENU_1 }, { T_HWSPI_CS_IDLE_MENU_2 } };

uint32_t hw3wire_setup(void) {
    uint32_t temp;

    static const struct ui_prompt hw3wire_menu[] = { { .description = T_SPEED,
                                                   .menu_items = hw3wire_speed_menu,
                                                   .menu_items_count = count_of(hw3wire_speed_menu),
                                                   .prompt_text = T_HWSPI_SPEED_PROMPT,
                                                   .minval = 1,
                                                   .maxval = 3900,
                                                   .defval = 100,
                                                   .menu_action = 0,
                                                   .config = &prompt_int_cfg },
                                                { .description = T_HWSPI_BITS_MENU,
                                                   .menu_items = hw3wire_bits_menu,
                                                   .menu_items_count = count_of(hw3wire_bits_menu),
                                                   .prompt_text = T_HWSPI_BITS_PROMPT,
                                                   .minval = 4,
                                                   .maxval = 8,
                                                   .defval = 8,
                                                   .menu_action = 0,
                                                   .config = &prompt_int_cfg },
                                                 { .description = T_HWSPI_CLOCK_POLARITY_MENU,
                                                   .menu_items = hw3wire_polarity_menu,
                                                   .menu_items_count = count_of(hw3wire_polarity_menu),
                                                   .prompt_text = T_HWSPI_CLOCK_POLARITY_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 1,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg },
                                                 { .description = T_HWSPI_CLOCK_PHASE_MENU,
                                                   .menu_items = hw3wire_phase_menu,
                                                   .menu_items_count = count_of(hw3wire_phase_menu),
                                                   .prompt_text = T_HWSPI_CLOCK_PHASE_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 1,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg },
                                                 { .description = T_HWSPI_CS_IDLE_MENU,
                                                   .menu_items = hw3wire_idle_menu,
                                                   .menu_items_count = count_of(hw3wire_idle_menu),
                                                   .prompt_text = T_HWSPI_CS_IDLE_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 2,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg } };
    prompt_result result;

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

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        hw3wire_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }

    ui_prompt_uint32(&result, &hw3wire_menu[0], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.baudrate = temp * 1000;
/*
    ui_prompt_uint32(&result, &hw3wire_menu[1], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.data_bits = (uint8_t)temp;
    system_config.num_bits = (uint8_t)temp;

    ui_prompt_uint32(&result, &hw3wire_menu[2], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.clock_polarity = (uint8_t)((temp - 1));

    ui_prompt_uint32(&result, &hw3wire_menu[3], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.clock_phase = (uint8_t)(temp - 1);
*/
    ui_prompt_uint32(&result, &hw3wire_menu[4], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.cs_idle = (uint8_t)(temp - 1);

    storage_save_mode(config_file, config_t, count_of(config_t));
    //}

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
    system_bio_claim(true, M_3WIRE_MOSI, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, M_3WIRE_SCLK, BP_PIN_MODE, pin_labels[1]);
    system_bio_claim(true, M_3WIRE_MISO, BP_PIN_MODE, pin_labels[2]);
    system_bio_claim(true, M_3WIRE_CS, BP_PIN_MODE, pin_labels[3]);
    //pio_hw3wire_reset();
    //bio_put(M_2WIRE_RST, 0); // preload the RST pin to be 0 when output
    hw3wire_set_cs(M_3WIRE_DESELECT);
}

void hw3wire_cleanup(void) {
    pio_hw3wire_cleanup();
    bio_init();
    system_bio_claim(false, M_3WIRE_MOSI, BP_PIN_MODE, 0);
    system_bio_claim(false, M_3WIRE_SCLK, BP_PIN_MODE, 0);
    system_bio_claim(false, M_3WIRE_MISO, BP_PIN_MODE, 0);
    system_bio_claim(false, M_3WIRE_CS, BP_PIN_MODE, 0);
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
    ui_prompt_mode_settings_int(GET_T(T_SPEED), mode_config.baudrate/1000, GET_T(T_KHZ));
    ui_prompt_mode_settings_string(GET_T(T_HWSPI_CS_IDLE_MENU), GET_T(hw3wire_idle_menu[mode_config.cs_idle].description), 0x00);
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
