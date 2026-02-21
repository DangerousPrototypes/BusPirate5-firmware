/**
 * @file hwspi.c
 * @brief Hardware SPI mode implementation.
 * @details Implements SPI master protocol using RP2040/RP2350 hardware SPI peripheral.
 *          Features:
 *          - Speed: 1kHz to 62.5MHz
 *          - Data bits: 4-8 bits
 *          - Clock polarity and phase control
 *          - Configurable CS idle state
 *          - Flash programming support (via SFUD library)
 *          - SPI EEPROM support
 *          
 *          Pin mapping:
 *          - CLK:  Clock output
 *          - MOSI: Master out, slave in
 *          - MISO: Master in, slave out
 *          - CS:   Chip select (active low by default)
 */

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
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "lib/sfud/inc/sfud.h"
#include "lib/sfud/inc/sfud_def.h"
#include "commands/spi/flash.h"
#include "ui/ui_help.h"
#include "pirate/hwspi.h"
//#include "commands/spi/sniff.h"
#include "usb_rx.h"
#include "commands/eeprom/eeprom_spi.h"
#include "lib/bp_args/bp_cmd.h"

// command configuration
const struct _mode_command_struct hwspi_commands[] = {
    {   .def=&flash_def,
        .func=&flash, 
        .supress_fala_capture=true
    },
    {   .def=&eeprom_spi_def,
        .func=&spi_eeprom_handler, 
        .supress_fala_capture=true

    },
#if 0
    {   .func=&sniff_handler,
        .def=&sniff_def,
        .supress_fala_capture=true
    },
#endif
};
const uint32_t hwspi_commands_count = count_of(hwspi_commands);

static const char pin_labels[][5] = { "CLK", "MOSI", "MISO", "CS" };

static struct _spi_mode_config mode_config;

// Speed — flag -s / --speed (kHz; stored as Hz)
static const bp_val_constraint_t spi_speed_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 625000, .def = 100 },
    .prompt = T_HWSPI_SPEED_MENU,
    .hint = T_HWSPI_SPEED_MENU_1,
};

// Data bits — flag -d / --databits
static const bp_val_constraint_t spi_databits_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 4, .max = 8, .def = 8 },
    .prompt = T_HWSPI_BITS_MENU,
    .hint = T_HWSPI_BITS_MENU_1,
};

// Clock polarity — flag -o / --polarity
static const bp_val_choice_t polarity_choices[] = {
    { "idle_low",  NULL, T_HWSPI_CLOCK_POLARITY_MENU_1, 0 },
    { "idle_high", NULL, T_HWSPI_CLOCK_POLARITY_MENU_2, 1 },
};
static const bp_val_constraint_t spi_polarity_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = polarity_choices, .count = 2, .def = 0 },
    .prompt = T_HWSPI_CLOCK_POLARITY_MENU,
};

// Clock phase — flag -a / --phase
static const bp_val_choice_t phase_choices[] = {
    { "leading",  NULL, T_HWSPI_CLOCK_PHASE_MENU_1, 0 },
    { "trailing", NULL, T_HWSPI_CLOCK_PHASE_MENU_2, 1 },
};
static const bp_val_constraint_t spi_phase_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = phase_choices, .count = 2, .def = 0 },
    .prompt = T_HWSPI_CLOCK_PHASE_MENU,
};

// CS idle — flag -c / --csidle
static const bp_val_choice_t csidle_choices[] = {
    { "low",  NULL, T_HWSPI_CS_IDLE_MENU_1, 0 },
    { "high", NULL, T_HWSPI_CS_IDLE_MENU_2, 1 },
};
static const bp_val_constraint_t spi_csidle_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = csidle_choices, .count = 2, .def = 1 },
    .prompt = T_HWSPI_CS_IDLE_MENU,
};

static const bp_command_opt_t spi_setup_opts[] = {
    { "speed",    's', BP_ARG_REQUIRED, "1-625000",              0, &spi_speed_range },
    { "databits", 'd', BP_ARG_REQUIRED, "4-8",                   0, &spi_databits_range },
    { "polarity", 'o', BP_ARG_REQUIRED, "idle_low/idle_high",    0, &spi_polarity_choice },
    { "phase",    'a', BP_ARG_REQUIRED, "leading/trailing",      0, &spi_phase_choice },
    { "csidle",   'c', BP_ARG_REQUIRED, "low/high",              0, &spi_csidle_choice },
    { 0 },
};

const bp_command_def_t spi_setup_def = {
    .name = "spi",
    .description = 0,
    .opts = spi_setup_opts,
};

uint32_t spi_setup(void) {
    uint32_t temp;

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

    // Detect interactive vs CLI mode by checking the primary flag
    bp_cmd_status_t st = bp_cmd_flag(&spi_setup_def, 's', &temp);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            spi_settings();
            int r = bp_cmd_yes_no_exit("");
            if (r == BP_YN_EXIT) return 0; // exit
            if (r == BP_YN_YES)  return 1; // use saved settings
        }

        if (bp_cmd_prompt(&spi_speed_range, &temp) != BP_CMD_OK) return 0;
        mode_config.baudrate = temp * 1000;

        if (bp_cmd_prompt(&spi_databits_range, &temp) != BP_CMD_OK) return 0;
        mode_config.data_bits = (uint8_t)temp;
        system_config.num_bits = (uint8_t)temp;

        if (bp_cmd_prompt(&spi_polarity_choice, &temp) != BP_CMD_OK) return 0;
        mode_config.clock_polarity = (uint8_t)temp;

        if (bp_cmd_prompt(&spi_phase_choice, &temp) != BP_CMD_OK) return 0;
        mode_config.clock_phase = (uint8_t)temp;

        if (bp_cmd_prompt(&spi_csidle_choice, &temp) != BP_CMD_OK) return 0;
        mode_config.cs_idle = (uint8_t)temp;
    } else {
        // Speed already parsed above — multiply by 1000 to convert kHz → Hz
        mode_config.baudrate = temp * 1000;

        st = bp_cmd_flag(&spi_setup_def, 'd', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.data_bits = (uint8_t)temp;
        system_config.num_bits = (uint8_t)temp;

        st = bp_cmd_flag(&spi_setup_def, 'o', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.clock_polarity = (uint8_t)temp;

        st = bp_cmd_flag(&spi_setup_def, 'a', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.clock_phase = (uint8_t)temp;

        st = bp_cmd_flag(&spi_setup_def, 'c', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.cs_idle = (uint8_t)temp;
    }

    mode_config.binmode = false;

    storage_save_mode(config_file, config_t, count_of(config_t));

    mode_config.baudrate_actual = spi_init(M_SPI_PORT, mode_config.baudrate);
    printf("\r\n%s%s:%s %ukHz",
            ui_term_color_notice(),
            GET_T(T_HWSPI_ACTUAL_SPEED_KHZ),
            ui_term_color_reset(),
            mode_config.baudrate_actual / 1000);

    return 1;
}

uint32_t spi_setup_exc(void) {
    // setup spi
    mode_config.read_with_write = false;
    mode_config.baudrate_actual = spi_init(M_SPI_PORT, mode_config.baudrate);
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
    ui_help_setting_int(GET_T(T_HWSPI_SPEED_MENU), mode_config.baudrate / 1000, GET_T(T_KHZ));
    ui_help_setting_int(GET_T(T_HWSPI_BITS_MENU), mode_config.data_bits, 0x00);
    ui_help_setting_string(GET_T(T_HWSPI_CLOCK_POLARITY_MENU), GET_T(polarity_choices[mode_config.clock_polarity].label), 0x00);
    ui_help_setting_string(GET_T(T_HWSPI_CLOCK_PHASE_MENU), GET_T(phase_choices[mode_config.clock_phase].label), 0x00);
    ui_help_setting_string(GET_T(T_HWSPI_CS_IDLE_MENU), GET_T(csidle_choices[mode_config.cs_idle].label), 0x00);
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


//-----------------------------------------
//
// Flatbuffer/binary access functions
//-----------------------------------------

bool bpio_hwspi_configure(bpio_mode_configuration_t *bpio_mode_config){
    if(bpio_mode_config->debug) printf("[SPI] Speed %d Hz\r\n", bpio_mode_config->speed);
    mode_config.baudrate=bpio_mode_config->speed; // convert to kHz
    mode_config.data_bits = bpio_mode_config->data_bits;
    mode_config.clock_polarity = bpio_mode_config->clock_polarity;
    mode_config.clock_phase = bpio_mode_config->clock_phase;
    mode_config.cs_idle = bpio_mode_config->chip_select_idle;
    return true;  
}
