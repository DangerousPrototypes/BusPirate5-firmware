#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "lib/sfud/inc/sfud.h"
#include "lib/sfud/inc/sfud_def.h"
#include "commands/spi/flash.h"
#include "ui/ui_help.h"
#include "pirate/hwspi.h"
#include "commands/spi/sniff.h"
#include "usb_rx.h"
#include "commands/eeprom/eeprom_spi.h"

// command configuration
const struct _mode_command_struct hwspi_commands[] = {
    {   .command="flash", 
        .func=&flash, 
        .description_text=T_HELP_CMD_FLASH, 
        .supress_fala_capture=true
    },
    {   .command="eeprom", 
        .func=&spi_eeprom_handler, 
        .description_text=T_HELP_SPI_EEPROM, 
        .supress_fala_capture=true

    },
    {   .command="sniff", 
        .func=&sniff_handler, 
        .description_text=T_SPI_CMD_SNIFF, 
        .supress_fala_capture=true
    },    
};
const uint32_t hwspi_commands_count = count_of(hwspi_commands);

static const char pin_labels[][5] = { "CLK", "MOSI", "MISO", "CS" };

static struct _spi_mode_config mode_config;

static const struct prompt_item spi_speed_menu[] = { { T_HWSPI_SPEED_MENU_1 } };
static const struct prompt_item spi_bits_menu[] = { { T_HWSPI_BITS_MENU_1 } };
static const struct prompt_item spi_polarity_menu[] = { { T_HWSPI_CLOCK_POLARITY_MENU_1 },
                                                        { T_HWSPI_CLOCK_POLARITY_MENU_2 } };
static const struct prompt_item spi_phase_menu[] = { { T_HWSPI_CLOCK_PHASE_MENU_1 }, { T_HWSPI_CLOCK_PHASE_MENU_2 } };
static const struct prompt_item spi_idle_menu[] = { { T_HWSPI_CS_IDLE_MENU_1 }, { T_HWSPI_CS_IDLE_MENU_2 } };

uint32_t spi_setup(void) {
    uint32_t temp;

    static const struct ui_prompt spi_menu[] = { { .description = T_HWSPI_SPEED_MENU,
                                                   .menu_items = spi_speed_menu,
                                                   .menu_items_count = count_of(spi_speed_menu),
                                                   .prompt_text = T_HWSPI_SPEED_PROMPT,
                                                   .minval = 1,
                                                   .maxval = 625000,
                                                   .defval = 100,
                                                   .menu_action = 0,
                                                   .config = &prompt_int_cfg },
                                                 { .description = T_HWSPI_BITS_MENU,
                                                   .menu_items = spi_bits_menu,
                                                   .menu_items_count = count_of(spi_bits_menu),
                                                   .prompt_text = T_HWSPI_BITS_PROMPT,
                                                   .minval = 4,
                                                   .maxval = 8,
                                                   .defval = 8,
                                                   .menu_action = 0,
                                                   .config = &prompt_int_cfg },
                                                 { .description = T_HWSPI_CLOCK_POLARITY_MENU,
                                                   .menu_items = spi_polarity_menu,
                                                   .menu_items_count = count_of(spi_polarity_menu),
                                                   .prompt_text = T_HWSPI_CLOCK_POLARITY_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 1,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg },
                                                 { .description = T_HWSPI_CLOCK_PHASE_MENU,
                                                   .menu_items = spi_phase_menu,
                                                   .menu_items_count = count_of(spi_phase_menu),
                                                   .prompt_text = T_HWSPI_CLOCK_PHASE_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 1,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg },
                                                 { .description = T_HWSPI_CS_IDLE_MENU,
                                                   .menu_items = spi_idle_menu,
                                                   .menu_items_count = count_of(spi_idle_menu),
                                                   .prompt_text = T_HWSPI_CS_IDLE_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 2,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg } };
    prompt_result result;

    const char config_file[] = "bpspi.bp";

    const mode_config_t config_t[] = {
        // clang-format off
        { "$.baudrate", &mode_config.baudrate, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.data_bits", &mode_config.data_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.stop_bits", &mode_config.clock_polarity, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.parity", &mode_config.clock_phase, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.cs_idle", &mode_config.cs_idle, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        spi_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }

    ui_prompt_uint32(&result, &spi_menu[0], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.baudrate = temp * 1000;

    ui_prompt_uint32(&result, &spi_menu[1], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.data_bits = (uint8_t)temp;
    system_config.num_bits = (uint8_t)temp;

    ui_prompt_uint32(&result, &spi_menu[2], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.clock_polarity = (uint8_t)((temp - 1));

    ui_prompt_uint32(&result, &spi_menu[3], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.clock_phase = (uint8_t)(temp - 1);

    ui_prompt_uint32(&result, &spi_menu[4], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.cs_idle = (uint8_t)(temp - 1);

    mode_config.binmode = false;

    storage_save_mode(config_file, config_t, count_of(config_t));
    //}

    return 1;
}

uint32_t spi_binmode_get_config_length(void) {
    return 8;
}

uint32_t spi_binmode_setup(uint8_t* config) {
    // spi config sequence:
    // 0x3b9aca0 4-8 CPOL=0 CPHA=0 CS=1
    uint32_t temp = 0;
    char c;
    bool error = false;
    for (uint8_t i = 0; i < 4; i++) {
        temp = temp << 8;
        temp |= config[i];
    }
    if (temp > 62500000) {
        error = true;
    } else {
        mode_config.baudrate = temp;
    }

    c = config[4];
    if (c < 4 || c > 8) {
        error = true;
    } else {
        mode_config.data_bits = c;
    }

    c = config[5];
    if (c > 1) {
        error = true;
    } else {
        mode_config.clock_polarity = c;
    }

    c = config[6];
    if (c > 1) {
        error = true;
    } else {
        mode_config.clock_phase = c;
    }

    c = config[7];
    if (c > 1) {
        error = true;
    } else {
        mode_config.cs_idle = c;
    }

    mode_config.binmode = true;

    return error;
}

uint32_t spi_setup_exc(void) {
    // setup spi
    mode_config.read_with_write = false;
    mode_config.baudrate_actual = spi_init(M_SPI_PORT, mode_config.baudrate);
    if (!mode_config.binmode) {
        printf("\r\n%s%s:%s %ukHz",
               ui_term_color_notice(),
               GET_T(T_HWSPI_ACTUAL_SPEED_KHZ),
               ui_term_color_reset(),
               mode_config.baudrate_actual / 1000);
    }
    hwspi_init(mode_config.data_bits, mode_config.clock_polarity, mode_config.clock_phase);
    system_bio_update_purpose_and_label(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, M_SPI_CS, BP_PIN_MODE, pin_labels[3]);
    spi_set_cs(M_SPI_DESELECT);
    return mode_config.baudrate_actual;
}

void spi_cleanup(void) {
    // disable peripheral
    hwspi_deinit();
    // release pin claims
    system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
    // update system_config pins
    system_config.misoport = 0;
    system_config.mosiport = 0;
    system_config.csport = 0;
    system_config.clkport = 0;
    system_config.misopin = 0;
    system_config.mosipin = 0;
    system_config.cspin = 0;
    system_config.clkpin = 0;
}

bool spi_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 0x00);
}

void spi_set_cs(uint8_t cs) {
    if (cs == M_SPI_SELECT) { // 'start'
        if (mode_config.cs_idle) {
            hwspi_select();
        } else {
            hwspi_deselect();
        }
    } else { // 'stop'
        if (mode_config.cs_idle) {
            hwspi_deselect();
        } else {
            hwspi_select();
        }
    }
}

void spi_start(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = false;
    result->data_message = GET_T(T_HWSPI_CS_SELECT);
    spi_set_cs(M_SPI_SELECT);
}

void spi_startr(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = true;
    result->data_message = GET_T(T_HWSPI_CS_SELECT);
    spi_set_cs(M_SPI_SELECT);
}

void spi_stop(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = false;
    result->data_message = GET_T(T_HWSPI_CS_DESELECT);
    spi_set_cs(M_SPI_DESELECT);
}

void spi_stopr(struct _bytecode* result, struct _bytecode* next) {
    mode_config.read_with_write = false;
    result->data_message = GET_T(T_HWSPI_CS_DESELECT);
    spi_set_cs(M_SPI_DESELECT);
}

void spi_write(struct _bytecode* result, struct _bytecode* next) {
    // hwspi_write((uint32_t)result->out_data);
    result->in_data = hwspi_write_read((uint8_t)result->out_data);
    result->read_with_write = mode_config.read_with_write;
}

void spi_read(struct _bytecode* result, struct _bytecode* next) {
    result->in_data = (uint8_t)hwspi_read();
}

void spi_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf(" 0. This menu\r\n");
            break;
        case 1:
            break;
        case 3:
            ui_term_detect();
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}
/*
void spi_pins(void)
{
    printf("CS\tMISO\tCLK\tMOSI");
}
*/

void spi_settings(void) {
    ui_prompt_mode_settings_int(GET_T(T_HWSPI_SPEED_MENU), mode_config.baudrate / 1000, GET_T(T_KHZ));
    ui_prompt_mode_settings_int(GET_T(T_HWSPI_BITS_MENU), mode_config.data_bits, 0x00);
    ui_prompt_mode_settings_string(GET_T(T_HWSPI_CLOCK_POLARITY_MENU),GET_T(spi_polarity_menu[mode_config.clock_polarity].description), 0x00);
    ui_prompt_mode_settings_string(GET_T(T_HWSPI_CLOCK_PHASE_MENU), GET_T(spi_phase_menu[mode_config.clock_phase].description), 0x00);
    ui_prompt_mode_settings_string(GET_T(T_HWSPI_CS_IDLE_MENU), GET_T(spi_idle_menu[mode_config.cs_idle].description), 0x00);
}

void spi_printSPIflags(void) {
}

void spi_help(void) {
    printf("Peer to peer 3 or 4 wire full duplex protocol. Very\r\n");
    printf("high clockrates upto 20MHz are possible.\r\n");
    printf("\r\n");
    printf("More info: https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus\r\n");
    printf("\r\n");

    printf("BPCMD\t {,] |                 DATA (1..32bit)               | },]\r\n");
    printf("CMD\tSTART| D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | STOP\r\n");

    if (mode_config.clock_phase) {
        printf("MISO\t-----|{###}|{###}|{###}|{###}|{###}|{###}|{###}|{###}|------\r\n");
        printf("MOSI\t-----|{###}|{###}|{###}|{###}|{###}|{###}|{###}|{###}|------\r\n");
    } else {
        printf("MISO\t---{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}--|------\r\n");
        printf("MOSI\t---{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}--|------\r\n");
    }

    if (mode_config.clock_polarity >> 1) {
        printf("CLK     "
               "\"\"\"\"\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"\"\"\"\"\r\n");
    } else {
        printf("CLK\t_____|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|______\r\n");
    }

    if (mode_config.cs_idle) {
        printf("CS\t\"\"___|_____|_____|_____|_____|_____|_____|_____|_____|___\"\"\"\r\n");
    } else {
        printf("CS\t__\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|"
               "\"\"\"___\r\n");
    }

    printf("\r\nCurrent mode is spi_clock_phase=%d and spi_clock_polarity=%d\r\n",
           mode_config.clock_phase,
           mode_config.clock_polarity >> 1);
    printf("\r\n");
    printf("Connection:\r\n");
    printf("\tMOSI \t------------------ MOSI\r\n");
    printf("\tMISO \t------------------ MISO\r\n");
    printf("{BP}\tCLK\t------------------ CLK\t{DUT}\r\n");
    printf("\tCS\t------------------ CS\r\n");
    printf("\tGND\t------------------ GND\r\n");

    ui_help_mode_commands(hwspi_commands, hwspi_commands_count);
}

uint32_t spi_get_speed(void) {
    return mode_config.baudrate_actual;
}