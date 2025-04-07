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
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "ui/ui_format.h"
#include "pirate/storage.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_help.h"
#include "commands/uart/nmea.h"
#include "commands/uart/bridge.h"
#include "commands/uart/monitor.h"
#include "commands/uart/glitch.h"

static struct _uart_mode_config mode_config;
static struct command_attributes periodic_attributes;

// command configuration
const struct _mode_command_struct hwuart_commands[] = {
    {   .command="gps", 
        .func=&nmea_decode_handler, 
        .description_text=T_HELP_UART_NMEA, 
        .supress_fala_capture=true
    },
    {   .command="bridge", 
        .func=&uart_bridge_handler, 
        .description_text=T_HELP_UART_BRIDGE, 
        .supress_fala_capture=true
    },
    {   .command="test", 
        .func=&uart_monitor_handler, 
        .description_text=T_UART_CMD_TEST, 
        .supress_fala_capture=false
    },
    {   .command="glitch", 
        .func=&uart_glitch_handler, 
        .description_text=T_HELP_UART_GLITCH, 
        .supress_fala_capture=false
    },
};
const uint32_t hwuart_commands_count = count_of(hwuart_commands);

static const char pin_labels[][5] = { "TX->", "RX<-", "RTS", "CTS" };

static const struct prompt_item uart_speed_menu[] = { { T_UART_SPEED_MENU_1 } };
static const struct prompt_item uart_parity_menu[] = { { T_UART_PARITY_MENU_1 },
                                                       { T_UART_PARITY_MENU_2 },
                                                       { T_UART_PARITY_MENU_3 } };
static const struct prompt_item uart_data_bits_menu[] = { { T_UART_DATA_BITS_MENU_1 } };
static const struct prompt_item uart_stop_bits_menu[] = { { T_UART_STOP_BITS_MENU_1 }, { T_UART_STOP_BITS_MENU_2 } };
static const struct prompt_item uart_blocking_menu[] = { { T_UART_BLOCKING_MENU_1 }, { T_UART_BLOCKING_MENU_2 } };
static const struct prompt_item uart_flow_control_menu[] = { { T_UART_FLOW_CONTROL_MENU_1 },
                                                             { T_UART_FLOW_CONTROL_MENU_2 } };
static const struct prompt_item uart_invert_menu[] = { { T_UART_INVERT_MENU_1 }, { T_UART_INVERT_MENU_2 } };

static const struct ui_prompt uart_menu[] = { [0] = { .description = T_UART_SPEED_MENU,
                                                      .menu_items = uart_speed_menu,
                                                      .menu_items_count = count_of(uart_speed_menu),
                                                      .prompt_text = T_UART_SPEED_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 7372800,
                                                      .defval = 115200,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [1] = { .description = T_UART_PARITY_MENU,
                                                      .menu_items = uart_parity_menu,
                                                      .menu_items_count = count_of(uart_parity_menu),
                                                      .prompt_text = T_UART_PARITY_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 0,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_list_cfg },
                                              [2] = { .description = T_UART_DATA_BITS_MENU,
                                                      .menu_items = uart_data_bits_menu,
                                                      .menu_items_count = count_of(uart_data_bits_menu),
                                                      .prompt_text = T_UART_DATA_BITS_PROMPT,
                                                      .minval = 5,
                                                      .maxval = 8,
                                                      .defval = 8,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [3] = { .description = T_UART_STOP_BITS_MENU,
                                                      .menu_items = uart_stop_bits_menu,
                                                      .menu_items_count = count_of(uart_stop_bits_menu),
                                                      .prompt_text = T_UART_STOP_BITS_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 0,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_list_cfg },
                                              [4] = { .description = T_UART_BLOCKING_MENU,
                                                      .menu_items = uart_blocking_menu,
                                                      .menu_items_count = count_of(uart_blocking_menu),
                                                      .prompt_text = T_UART_BLOCKING_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 0,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_list_cfg },
                                              [5] = { .description = T_UART_FLOW_CONTROL_MENU,
                                                      .menu_items = uart_flow_control_menu,
                                                      .menu_items_count = count_of(uart_flow_control_menu),
                                                      .prompt_text = T_UART_FLOW_CONTROL_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 0,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_list_cfg },
                                              [6] = { .description = T_UART_INVERT_MENU,
                                                      .menu_items = uart_invert_menu,
                                                      .menu_items_count = count_of(uart_invert_menu),
                                                      .prompt_text = T_UART_INVERT_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 0,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_list_cfg } };

uint32_t hwuart_setup(void) {
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

    prompt_result result;

    const char config_file[] = "bpuart.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.baudrate", &mode_config.baudrate, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.data_bits", &mode_config.data_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.stop_bits", &mode_config.stop_bits, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.parity", &mode_config.parity, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.flow_ctrl", &mode_config.flow_control, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.invert", &mode_config.invert, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format off
    };

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        hwuart_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }

    ui_prompt_uint32(&result, &uart_menu[0], &mode_config.baudrate);
    if (result.exit) {
        return 0;
    }

    ui_prompt_uint32(&result, &uart_menu[2], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.data_bits = (uint8_t)temp;

    ui_prompt_uint32(&result, &uart_menu[1], &temp); // could also just subtract one...
    if (result.exit) {
        return 0;
    }
    mode_config.parity = (uint8_t)temp;
    // uart_parity_t { UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD }
    // subtract 1 for actual parity setting
    mode_config.parity--;

    ui_prompt_uint32(&result, &uart_menu[3], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.stop_bits = (uint8_t)temp;
    // block=(ui_prompt_int(UARTBLOCKINGMENU, 1, 2, 2)-1);

    ui_prompt_uint32(&result, &uart_menu[5], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.flow_control = temp;
    mode_config.flow_control--;

    ui_prompt_uint32(&result, &uart_menu[6], &temp);
    if (result.exit) {
        return 0;
    }
    mode_config.invert = temp;
    mode_config.invert--;

    storage_save_mode(config_file, config_t, count_of(config_t));

    mode_config.async_print = false;

    return 1;
}

uint32_t hwuart_setup_exc(void) {
    // setup peripheral
    mode_config.baudrate_actual = uart_init(M_UART_PORT, mode_config.baudrate);
    printf("\r\n%s%s: %u %s%s",
           ui_term_color_notice(),
           GET_T(T_UART_ACTUAL_SPEED_BAUD),
           mode_config.baudrate_actual,
           GET_T(T_UART_BAUD),
           ui_term_color_reset());
    uart_set_format(M_UART_PORT, mode_config.data_bits, mode_config.stop_bits, mode_config.parity);
    // set buffers to correct position
    bio_buf_output(M_UART_TX); // tx
    bio_buf_input(M_UART_RX);  // rx
    // assign peripheral to io pins
    bio_set_function(M_UART_TX, GPIO_FUNC_UART); // tx
    bio_set_function(M_UART_RX, GPIO_FUNC_UART); // rx
    system_bio_update_purpose_and_label(true, M_UART_TX, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_UART_RX, BP_PIN_MODE, pin_labels[1]);

    bio_set_function(M_UART_CTS, GPIO_FUNC_UART);
    bio_set_function(M_UART_RTS, GPIO_FUNC_SIO);
    bio_output(M_UART_RTS);
    if (mode_config.flow_control) {
        // only show the pins if flow control is enabled in order to avoid confusion
        system_bio_update_purpose_and_label(true, M_UART_RTS, BP_PIN_MODE, pin_labels[2]);
        system_bio_update_purpose_and_label(true, M_UART_CTS, BP_PIN_MODE, pin_labels[3]);
    }
    gpio_set_inover(bio2bufiopin[M_UART_CTS], mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_outover(bio2bufiopin[M_UART_RTS], mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    // 0: ready to receive
    // 1: not ready to receive
    bio_put(M_UART_RTS, 1);
    // only enable CTS, as we are toggling RTS manually
    //
    // hw_flow RTS doesn't work with inverted signals, i.e. doesn't set RTS high
    // when we can read the data. So we're setting RTS manually.
    uart_set_hw_flow(M_UART_PORT, mode_config.flow_control, false);
    gpio_set_outover(bio2bufiopin[M_UART_TX], mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_inover(bio2bufiopin[M_UART_RX], mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);

    return 1;
}

bool hwuart_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 0x00);
}

void hwuart_periodic(void) {
    if (mode_config.async_print && uart_is_readable(M_UART_PORT)) {
        // printf("ASYNC: %d\r\n", uart_getc(M_UART_PORT));
        bio_put(M_UART_RTS, 0);
        uint32_t temp = uart_getc(M_UART_PORT);
        bio_put(M_UART_RTS, 1);
        ui_format_print_number_2(&periodic_attributes, &temp);
    }
}

void hwuart_open(struct _bytecode* result, struct _bytecode* next) {    
    // clear FIFO and enable UART
    bio_put(M_UART_RTS, 0);
    while (uart_is_readable(M_UART_PORT)) {
        uart_getc(M_UART_PORT);
    }
    bio_put(M_UART_RTS, 1);

    mode_config.async_print = false;
    result->data_message = GET_T(T_UART_OPEN);
}

void hwuart_open_read(struct _bytecode* result, struct _bytecode* next) { // start with read
    mode_config.async_print = true;
    result->data_message = GET_T(T_UART_OPEN_WITH_READ);
}

void hwuart_close(struct _bytecode* result, struct _bytecode* next) {
    mode_config.async_print = false;
    result->data_message = GET_T(T_UART_CLOSE);
}

void hwuart_write(struct _bytecode* result, struct _bytecode* next) {       
    if (mode_config.blocking) {
        uart_putc_raw(M_UART_PORT, result->out_data);
    } else {
        uart_putc_raw(M_UART_PORT, result->out_data);
    }
}

void hwuart_read(struct _bytecode* result, struct _bytecode* next) {
    uint32_t timeout = 0xfff;

    while (!uart_is_readable(M_UART_PORT)) {
        timeout--;
        if (!timeout) {
            result->error = SERR_ERROR;
            result->error_message = GET_T(T_UART_NO_DATA_READ);
            return;
        }
    }

    bio_put(M_UART_RTS, 0);
    if (uart_is_readable(M_UART_PORT)) {
        result->in_data = uart_getc(M_UART_PORT);
    } else {
        result->error = SERR_ERROR;
        result->error_message = GET_T(T_UART_NO_DATA_READ);
    }
    bio_put(M_UART_RTS, 1);
}

void hwuart_macro(uint32_t macro) {
    struct command_result result;
    switch (macro) {
        case 0:
            printf("1. Transparent UART bridge\r\n");
            break;
        case 1:
            uart_bridge_handler(&result);
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}

void hwuart_cleanup(void) {
    // disable peripheral
    uart_deinit(M_UART_PORT);
    system_bio_update_purpose_and_label(false, M_UART_TX, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UART_RX, BP_PIN_MODE, 0);
    // reset all pins to safe mode (done before mode change, but we do it here to be safe)
    bio_init();
    // update modeConfig pins
    system_config.misoport = 0;
    system_config.mosiport = 0;
    system_config.misopin = 0;
    system_config.mosipin = 0;
}

void hwuart_settings(void) {
    ui_prompt_mode_settings_int(GET_T(T_UART_SPEED_MENU), mode_config.baudrate, GET_T(T_UART_BAUD));
    ui_prompt_mode_settings_int(GET_T(T_UART_DATA_BITS_MENU), mode_config.data_bits, 0x00);
    ui_prompt_mode_settings_string(GET_T(T_UART_PARITY_MENU), GET_T(uart_parity_menu[mode_config.parity].description), 0x00);
    ui_prompt_mode_settings_int(GET_T(T_UART_STOP_BITS_MENU), mode_config.stop_bits, 0x00);
    ui_prompt_mode_settings_string(GET_T(T_UART_FLOW_CONTROL_MENU),
            !mode_config.flow_control ? GET_T(T_UART_FLOW_CONTROL_MENU_1) : GET_T(T_UART_FLOW_CONTROL_MENU_2), 0x00);
    ui_prompt_mode_settings_string(GET_T(T_UART_INVERT_MENU),
            !mode_config.invert ? GET_T(T_UART_INVERT_MENU_1) : GET_T(T_UART_INVERT_MENU_2), 0x00);
}

void hwuart_printerror(void) {
}

void hwuart_help(void) {
    printf("Peer to peer asynchronous protocol.\r\n");
    printf("\r\n");

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
    printf("\tTXD\t------------------ RXD\r\n");
    printf("{BP}\tRXD\t------------------ TXD\t{DUT}\r\n");
    printf("\tGND\t------------------ GND\r\n\r\n");

    printf("%s{\tuse { to print data as it arrives\r\n}/]\t use } or ] to stop printing data\r\n",
           ui_term_color_info());

    ui_help_mode_commands(hwuart_commands, hwuart_commands_count);
}

void hwuart_wait_done(void) {
    uart_tx_wait_blocking(M_UART_PORT);
}

uint32_t hwuart_get_speed(void) {
    return mode_config.baudrate_actual;
}
