#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/bio.h"
#include "ui/ui_term.h"
#include "ui/ui_format.h"
#include "pirate/storage.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_help.h"
#include "commands/uart/nmea.h"
#include "commands/uart/bridge.h"
#include "commands/uart/simcard.h"
#include "pirate/hwuart_pio.h"
#include "commands/hdplxuart/bridge.h"
#include "mode/hwhduart.h"
#include "lib/bp_args/bp_cmd.h"
static struct _uart_mode_config mode_config;
static struct command_attributes periodic_attributes;

// command configuration
const struct _mode_command_struct hwhduart_commands[] = { // Function Help
    /*{ .command="sim", 
        .func=&simcard_handler, 
        .supress_fala_capture=false
    },*/
    {   .func=&hduart_bridge_handler, 
        .def=&hduart_bridge_def,
        .supress_fala_capture=true
    },    
};
const uint32_t hwhduart_commands_count = count_of(hwhduart_commands);

static const char pin_labels[][5] = {
    "RXTX",
    "RST", // TODO: support reset like 2 wire mode
    "CTS",
    "RTS",
};

// Baud rate — flag -b / --baud
static const bp_val_constraint_t hduart_baud_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 1000000, .def = 115200 },
    .prompt = T_UART_SPEED_MENU,
    .hint = T_UART_SPEED_MENU_1,
};

// Data bits — flag -d / --databits
static const bp_val_constraint_t hduart_databits_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 5, .max = 8, .def = 8 },
    .prompt = T_UART_DATA_BITS_MENU,
    .hint = T_UART_DATA_BITS_MENU_1,
};

// Parity — flag -p / --parity
static const bp_val_choice_t hduart_parity_choices[] = {
    { "none", "n", T_UART_PARITY_MENU_1, 0 },
    { "even", "e", T_UART_PARITY_MENU_2, 1 },
    { "odd",  "o", T_UART_PARITY_MENU_3, 2 },
};
static const bp_val_constraint_t hduart_parity_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = hduart_parity_choices, .count = 3, .def = 0 },
    .prompt = T_UART_PARITY_MENU,
};

// Stop bits — flag -s / --stopbits
static const bp_val_choice_t hduart_stopbits_choices[] = {
    { "1", NULL, T_UART_STOP_BITS_MENU_1, 1 },
    { "2", NULL, T_UART_STOP_BITS_MENU_2, 2 },
};
static const bp_val_constraint_t hduart_stopbits_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = hduart_stopbits_choices, .count = 2, .def = 1 },
    .prompt = T_UART_STOP_BITS_MENU,
};

static const bp_command_opt_t hduart_setup_opts[] = {
    { "baud",     'b', BP_ARG_REQUIRED, "1-1000000",     0, &hduart_baud_range },
    { "databits", 'd', BP_ARG_REQUIRED, "5-8",           0, &hduart_databits_range },
    { "parity",   'p', BP_ARG_REQUIRED, "none/even/odd", 0, &hduart_parity_choice },
    { "stopbits", 's', BP_ARG_REQUIRED, "1/2",           0, &hduart_stopbits_choice },
    { 0 },
};

const bp_command_def_t hduart_setup_def = {
    .name = "hduart",
    .description = 0,
    .opts = hduart_setup_opts,
};

uint32_t hwhduart_setup(void) {
    uint32_t temp;
    periodic_attributes.has_value = true;
    periodic_attributes.has_dot = false;
    periodic_attributes.has_colon = false;
    periodic_attributes.has_string = false;
    periodic_attributes.command = 0;       // the actual command called
    periodic_attributes.number_format = 4; // DEC/HEX/BIN
    periodic_attributes.value = 0;         // integer value parsed from command line
    periodic_attributes.dot = 0;           // value after .
    periodic_attributes.colon = 0;         // value after :

    const char config_file[] = "bphduart.bp";

    const mode_config_t config_t[] = {
        // clang-format off
        { "$.baudrate", &mode_config.baudrate, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.data_bits", &mode_config.data_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.stop_bits", &mode_config.stop_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.parity", &mode_config.parity, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    // Detect interactive vs CLI mode by checking the primary flag
    bp_cmd_status_t st = bp_cmd_flag(&hduart_setup_def, 'b', &mode_config.baudrate);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            hwhduart_settings();
            int r = bp_cmd_yes_no_exit("");
            if (r == BP_YN_EXIT) return 0; // exit
            if (r == BP_YN_YES)  return 1; // use saved settings
        }

        if (bp_cmd_prompt(&hduart_baud_range, &mode_config.baudrate) != BP_CMD_OK) return 0;

        if (bp_cmd_prompt(&hduart_databits_range, &temp) != BP_CMD_OK) return 0;
        mode_config.data_bits = (uint8_t)temp;

        if (bp_cmd_prompt(&hduart_parity_choice, &temp) != BP_CMD_OK) return 0;
        mode_config.parity = (uint8_t)temp;

        if (bp_cmd_prompt(&hduart_stopbits_choice, &temp) != BP_CMD_OK) return 0;
        mode_config.stop_bits = (uint8_t)temp;
    } else {
        st = bp_cmd_flag(&hduart_setup_def, 'd', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.data_bits = (uint8_t)temp;

        st = bp_cmd_flag(&hduart_setup_def, 'p', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.parity = (uint8_t)temp;

        st = bp_cmd_flag(&hduart_setup_def, 's', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.stop_bits = (uint8_t)temp;
    }

    storage_save_mode(config_file, config_t, count_of(config_t));

    mode_config.async_print = false;

    return 1;
}

uint32_t hwhduart_setup_exc(void) {
    // setup peripheral
    // half duplex
    hwuart_pio_init(mode_config.data_bits, mode_config.parity, mode_config.stop_bits, mode_config.baudrate);
    system_bio_update_purpose_and_label(true, M_UART_RXTX, BP_PIN_MODE, pin_labels[0]);
    return 1;
}

bool hwhduart_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 1<<M_UART_RXTX);
}

void hwhduart_periodic(void) {
    uint32_t raw;
    uint8_t cooked;
    if (mode_config.async_print && hwuart_pio_read(&raw, &cooked)) {
        // printf("PIO: 0x%04X ", raw);
        printf("0x%02x ", cooked);
        // ui_format_print_number_2(&periodic_attributes,  &cooked);
    }
}

void hwhduart_open(struct _bytecode* result, struct _bytecode* next) {
    // clear FIFO and enable UART
    // while(uart_is_readable(M_UART_PORT)){
    //	uart_getc(M_UART_PORT);
    // }
    mode_config.async_print = true;
    result->data_message = GET_T(T_UART_OPEN_WITH_READ);
}

void hwhduart_open_read(struct _bytecode* result, struct _bytecode* next) { // start with read
    mode_config.async_print = true;
    result->data_message = GET_T(T_UART_OPEN_WITH_READ);
}

void hwhduart_close(struct _bytecode* result, struct _bytecode* next) {
    mode_config.async_print = false;
    result->data_message = GET_T(T_UART_CLOSE);
}

void hwhduart_start_alt(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HW2WIRE_RST_HIGH);
    bio_input(M_2WIRE_RST);
}

void hwhduart_stop_alt(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HW2WIRE_RST_LOW);
    bio_output(M_2WIRE_RST);
}

void hwhduart_write(struct _bytecode* result, struct _bytecode* next) {
    if (mode_config.blocking) {
        // uart_putc_raw(M_UART_PORT, result->out_data);
    } else {
        // uart_putc_raw(M_UART_PORT, result->out_data);
    }
    hwuart_pio_write(result->out_data);
}

void hwhduart_read(struct _bytecode* result, struct _bytecode* next) {
    uint32_t timeout = 0xfff;
    uint32_t raw;
    uint8_t cooked;

    while (!hwuart_pio_read(&raw, &cooked)) {
        timeout--;
        if (!timeout) {
            result->error = SERR_ERROR;
            result->error_message = GET_T(T_UART_NO_DATA_READ);
            return;
        }
    }
    result->in_data = cooked;
}

void hwhduart_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf("1. Transparent UART bridge\r\n");
            break;
        case 1:
            // uart_bridge_handler(&result);
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}

void hwhduart_cleanup(void) {
    // disable peripheral
    // half duplex
    hwuart_pio_deinit();
    system_bio_update_purpose_and_label(false, M_UART_RXTX, BP_PIN_MODE, 0);

    // reset all pins to safe mode (done before mode change, but we do it here to be safe)
    bio_init();
}

void hwhduart_settings(void) {
    //printf(" %s: %d %s\r\n", GET_T(T_UART_SPEED_MENU), mode_config.baudrate, GET_T(T_UART_BAUD) );
    ui_help_setting_int(GET_T(T_UART_SPEED_MENU), mode_config.baudrate, GET_T(T_UART_BAUD));
    //printf(" %s: %d\r\n", GET_T(T_UART_DATA_BITS_MENU), mode_config.data_bits);
    ui_help_setting_int(GET_T(T_UART_DATA_BITS_MENU), mode_config.data_bits, 0x00);
    ui_help_setting_string(GET_T(T_UART_PARITY_MENU), GET_T(hduart_parity_choices[mode_config.parity].label), 0x00);
    //printf(" %s: %d\r\n", GET_T(T_UART_STOP_BITS_MENU), mode_config.stop_bits);
    ui_help_setting_int(GET_T(T_UART_STOP_BITS_MENU), mode_config.stop_bits, 0x00);

}

void hwhduart_printerror(void) {
}

void hwhduart_help(void) {
    printf("Peer to peer HALF DUPLEX asynchronous protocol with open drain bus.\r\n");
    printf("Requires pull-up resistors\r\n\r\n");

    if (mode_config.parity == UART_PARITY_NONE) {
        printf("BPCMD\t     |                      DATA(8 bits)               |\r\n");
        printf("\tIDLE |STRT| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |STOP|IDLE\r\n");
        printf("TXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
        printf("RXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
    } else {
        printf("BPCMD\t     |                      DATA(8/9 bits)                  |\r\n");
        printf("\tIDLE |STRT| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |PRTY|STOP|IDLE\r\n");
        printf("TXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
        printf("RXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
    }

    printf("\t              ^sample moment\r\n");
    printf("\r\n");
    printf("Connections:\r\n");
    printf("\tRXTX\t------------------ RXTX\r\n");
    printf("\tGND\t------------------ GND\r\n\r\n");

    printf("%s{\tuse { to print data as it arrives\r\n}/]\t use } or ] to stop printing data\r\n",
           ui_term_color_info());

    ui_help_mode_commands(hwhduart_commands, hwhduart_commands_count);
}

uint32_t hwhduart_get_speed(void) {
    return mode_config.baudrate;
}
