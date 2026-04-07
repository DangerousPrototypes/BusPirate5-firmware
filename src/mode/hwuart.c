/**
 * @file hwuart.c
 * @brief Hardware UART mode implementation.
 * @details Implements UART (serial) protocol using PIO-based full-duplex communication.
 *          Features:
 *          - Baud rate: 300 to 1,000,000 bps (configurable)
 *          - Data bits: 5-8
 *          - Parity: None, Even, Odd
 *          - Stop bits: 1 or 2
 *          - Hardware flow control (RTS/CTS)
 *          - Signal inversion support
 *          - Async data printing
 *          - GPS NMEA decoder
 *          - UART bridge mode
 *          - Monitor mode for testing
 *          - Glitch testing
 *          
 *          Pin mapping:
 *          - TX:  Transmit output
 *          - RX:  Receive input
 *          - RTS: Request to send (flow control)
 *          - CTS: Clear to send (flow control)
 */

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
#include "commands/uart/monitor.h"
#include "commands/uart/glitch.h"
#include "lib/bp_args/bp_cmd.h"
#include "pirate/hwuart_pio.h"

static struct _uart_mode_config mode_config;
static struct command_attributes periodic_attributes;

// Backend actually brought up by hwuart_setup_exc(). Kept separate from
// mode_config.use_pio because the mode system calls the new mode's setup()
// (which rewrites mode_config) before the old mode's cleanup().
static bool pio_active = false;
static uint8_t pio_active_tx_pin = 0;
static uint8_t pio_active_rx_pin = 0;

bool hwuart_is_pio_mode(void) {
    return pio_active;
}

bool bpio_hwuart_configure(bpio_mode_configuration_t *bpio_mode_config){
    if(bpio_mode_config->debug) printf("[UART] Configuring - Speed %d baud, %d%c%d\r\n", 
        bpio_mode_config->speed,
        bpio_mode_config->data_bits,
        bpio_mode_config->parity ? 'E' : 'N',
        bpio_mode_config->stop_bits);
    
    mode_config.baudrate = bpio_mode_config->speed;
    mode_config.data_bits = bpio_mode_config->data_bits;
    mode_config.stop_bits = bpio_mode_config->stop_bits;
    mode_config.parity = bpio_mode_config->parity ? UART_PARITY_EVEN : UART_PARITY_NONE;
    mode_config.flow_control = bpio_mode_config->flow_control ? 1 : 0;
    mode_config.invert = bpio_mode_config->signal_inversion ? 1 : 0;
    mode_config.async_print = false; // Disabled by default, async data handled via BPIO packets
    mode_config.blocking = 0; // Non-blocking
    // Default to the hardware UART. Set before validation so a rejected request
    // never leaves a stale PIO configuration behind for setup_exc().
    mode_config.use_pio = false;

    if (bpio_mode_config->pin_tx != 255 && bpio_mode_config->pin_rx != 255) {
        if (bpio_mode_config->pin_tx > 7 || bpio_mode_config->pin_rx > 7) {
            if (bpio_mode_config->debug) printf("[UART] Invalid pin number (must be 0-7)\r\n");
            return false;
        }
        if (bpio_mode_config->pin_tx == bpio_mode_config->pin_rx) {
            if (bpio_mode_config->debug) printf("[UART] TX and RX pins must be different\r\n");
            return false;
        }
        if (bpio_mode_config->flow_control) {
            if (bpio_mode_config->debug) printf("[UART] Flow control not supported with PIO pins\r\n");
            return false;
        }
        mode_config.use_pio = true;
        mode_config.pio_tx_pin = bpio_mode_config->pin_tx;
        mode_config.pio_rx_pin = bpio_mode_config->pin_rx;
        if (bpio_mode_config->debug) printf("[UART] Using PIO on TX=BIO%d, RX=BIO%d\r\n",
            mode_config.pio_tx_pin, mode_config.pio_rx_pin);
    } else if (bpio_mode_config->pin_tx != 255 || bpio_mode_config->pin_rx != 255) {
        if (bpio_mode_config->debug) printf("[UART] Both pin_tx and pin_rx must be specified\r\n");
        return false;
    }

    return true;
}

// command configuration
const struct _mode_command_struct hwuart_commands[] = {
    {   .func=&nmea_decode_handler,
        .def=&nmea_decode_def,
        .supress_fala_capture=true
    },
    {   .func=&uart_bridge_handler,
        .def=&uart_bridge_def,
        .supress_fala_capture=true
    },
    /*{   .func=&uart_monitor_handler, 
        .supress_fala_capture=false
    },*/
    {   .func=&uart_glitch_handler,
        .def=&uart_glitch_def,
        .supress_fala_capture=false
    },
};
const uint32_t hwuart_commands_count = count_of(hwuart_commands);

static const char pin_labels[][5] = { "TX->", "RX<-", "RTS", "CTS" };

/*
 * =============================================================================
 * Constraint-based mode setup
 * =============================================================================
 * Command-line usage:
 *   m uart                              → full interactive wizard
 *   m uart -b 115200                    → 115200 8N1, all defaults
 *   m uart -b 115200 --parity even      → override one thing
 *   m uart -b 9600 -d 7 -p odd -s 2    → override multiple
 *
 * All parameters are flags. No positionals.
 */

// Baud rate — flag -b / --baud
static const bp_val_constraint_t uart_baud_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = 7372800, .def = 115200 },
    .prompt = T_UART_SPEED_MENU,
    .hint = T_UART_SPEED_MENU_1,
};

// Data bits — flag -d / --databits
static const bp_val_constraint_t uart_databits_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 5, .max = 8, .def = 8 },
    .prompt = T_UART_DATA_BITS_MENU,
    .hint = T_UART_DATA_BITS_MENU_1,
};

// Parity — flag -p / --parity (none/even/odd)
static const bp_val_choice_t parity_choices[] = {
    { "none", "n", T_UART_PARITY_MENU_1, 0 },  // UART_PARITY_NONE
    { "even", "e", T_UART_PARITY_MENU_2, 1 },  // UART_PARITY_EVEN
    { "odd",  "o", T_UART_PARITY_MENU_3, 2 },  // UART_PARITY_ODD
};
static const bp_val_constraint_t uart_parity_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = parity_choices, .count = 3, .def = 0 }, // default = none
    .prompt = T_UART_PARITY_MENU,
};

// Stop bits — flag -s / --stopbits
static const bp_val_choice_t stopbits_choices[] = {
    { "1", NULL, T_UART_STOP_BITS_MENU_1, 1 },
    { "2", NULL, T_UART_STOP_BITS_MENU_2, 2 },
};
static const bp_val_constraint_t uart_stopbits_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = stopbits_choices, .count = 2, .def = 1 },
    .prompt = T_UART_STOP_BITS_MENU,
};

// Flow control — flag -f / --flow
static const bp_val_choice_t flow_choices[] = {
    { "off",  NULL, T_UART_FLOW_CONTROL_MENU_1, 0 },
    { "rts",  NULL, T_UART_FLOW_CONTROL_MENU_2, 1 },
};
static const bp_val_constraint_t uart_flow_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = flow_choices, .count = 2, .def = 0 },
    .prompt = T_UART_FLOW_CONTROL_MENU,
};

// Signal inversion — flag -i / --invert
static const bp_val_choice_t invert_choices[] = {
    { "normal", NULL, T_UART_INVERT_MENU_1, 0 },
    { "invert", NULL, T_UART_INVERT_MENU_2, 1 },
};
static const bp_val_constraint_t uart_invert_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = invert_choices, .count = 2, .def = 0 },
    .prompt = T_UART_INVERT_MENU,
};

// TX pin — flag --tx (PIO mode, 0-7; 255 = default hardware pin)
static const bp_val_constraint_t uart_tx_pin_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 0, .max = 7, .def = 255 },
};

// RX pin — flag --rx (PIO mode, 0-7; 255 = default hardware pin)
static const bp_val_constraint_t uart_rx_pin_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 0, .max = 7, .def = 255 },
};

static const bp_command_opt_t uart_setup_opts[] = {
    { "baud",     'b', BP_ARG_REQUIRED, "1-7372800",     0, &uart_baud_range },
    { "databits", 'd', BP_ARG_REQUIRED, "5-8",           0, &uart_databits_range },
    { "parity",   'p', BP_ARG_REQUIRED, "none|even|odd", 0, &uart_parity_choice },
    { "stopbits", 's', BP_ARG_REQUIRED, "1|2",           0, &uart_stopbits_choice },
    { "flow",     'f', BP_ARG_REQUIRED, "off|rts",       0, &uart_flow_choice },
    { "invert",   'i', BP_ARG_REQUIRED, "normal|invert", 0, &uart_invert_choice },
    { "tx",       't', BP_ARG_REQUIRED, "0-7",           0, &uart_tx_pin_range },
    { "rx",       'r', BP_ARG_REQUIRED, "0-7",           0, &uart_rx_pin_range },
    { 0 },
};

const bp_command_def_t uart_setup_def = {
    .name = "uart",
    .description = 0,
    .opts = uart_setup_opts,
};

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

    // PIO pins are never saved/restored: they must be requested explicitly via --tx/--rx.
    // Parse them first so they apply on every path below, including saved settings.
    mode_config.use_pio = false;
    uint32_t tx_pin = 255, rx_pin = 255;
    bp_cmd_status_t st = bp_cmd_flag(&uart_setup_def, 't', &tx_pin);
    if (st == BP_CMD_INVALID) return 0;
    st = bp_cmd_flag(&uart_setup_def, 'r', &rx_pin);
    if (st == BP_CMD_INVALID) return 0;

    if (tx_pin != 255 && rx_pin != 255) {
        if (tx_pin == rx_pin) {
            printf("Error: TX and RX pins must be different\r\n");
            return 0;
        }
        mode_config.use_pio = true;
        mode_config.pio_tx_pin = (uint8_t)tx_pin;
        mode_config.pio_rx_pin = (uint8_t)rx_pin;
    } else if (tx_pin != 255 || rx_pin != 255) {
        printf("Error: Both --tx and --rx must be specified\r\n");
        return 0;
    }

    // Check if any flag is present — if so, command-line mode; otherwise wizard
    st = bp_cmd_flag(&uart_setup_def, 'b', &mode_config.baudrate);
    if (st == BP_CMD_INVALID) return 0;

    bool interactive = (st == BP_CMD_MISSING);
    bool use_saved = false;

    if (interactive) {

        // check for saved config and offer to use it
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            hwuart_settings();
            int r = bp_cmd_yes_no_exit("");
            if (r == BP_YN_EXIT) return 0; // exit
            if (r == BP_YN_YES)  use_saved = true; // use saved settings (PIO pins from --tx/--rx still apply)
        }

        if (!use_saved) {
            // ── Full interactive wizard ──
            if (bp_cmd_prompt(&uart_baud_range, &mode_config.baudrate) != BP_CMD_OK) return 0;

            if (bp_cmd_prompt(&uart_databits_range, &temp) != BP_CMD_OK) return 0;
            mode_config.data_bits = (uint8_t)temp;

            if (bp_cmd_prompt(&uart_parity_choice, &temp) != BP_CMD_OK) return 0;
            mode_config.parity = (uint8_t)temp;

            if (bp_cmd_prompt(&uart_stopbits_choice, &temp) != BP_CMD_OK) return 0;
            mode_config.stop_bits = (uint8_t)temp;

            if (mode_config.use_pio) {
                mode_config.flow_control = 0; // not supported on PIO pins, don't ask
            } else {
                if (bp_cmd_prompt(&uart_flow_choice, &temp) != BP_CMD_OK) return 0;
                mode_config.flow_control = temp;
            }

            if (bp_cmd_prompt(&uart_invert_choice, &temp) != BP_CMD_OK) return 0;
            mode_config.invert = temp;
        }
    } else {
        // ── Command-line mode ──
        // Baud already parsed. Remaining flags use defaults if absent.

        st = bp_cmd_flag(&uart_setup_def, 'd', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.data_bits = (uint8_t)temp;

        st = bp_cmd_flag(&uart_setup_def, 'p', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.parity = (uint8_t)temp;

        st = bp_cmd_flag(&uart_setup_def, 's', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.stop_bits = (uint8_t)temp;

        st = bp_cmd_flag(&uart_setup_def, 'f', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.flow_control = temp;

        st = bp_cmd_flag(&uart_setup_def, 'i', &temp);
        if (st == BP_CMD_INVALID) return 0;
        mode_config.invert = temp;
    }

    // Flow control (from flags or saved settings) needs the hardware UART pins
    if (mode_config.use_pio && mode_config.flow_control) {
        printf("Error: Flow control not supported with PIO pins\r\n");
        return 0;
    }

    if (use_saved) {
        return 1;
    }

    storage_save_mode(config_file, config_t, count_of(config_t));

    mode_config.async_print = false;

    if (mode_config.use_pio) {
        // PIO clock divider has a fractional part, the actual rate is within a fraction of a percent
        mode_config.baudrate_actual = mode_config.baudrate;
    } else {
        mode_config.baudrate_actual = uart_init(M_UART_PORT, mode_config.baudrate);
    }

    hwuart_settings();

    printf("\r\n%s%s: %u %s%s",
           ui_term_color_notice(),
           GET_T(T_UART_ACTUAL_SPEED_BAUD),
           mode_config.baudrate_actual,
           GET_T(T_UART_BAUD),
           ui_term_color_reset());

    return 1;
}

uint32_t hwuart_setup_exc(void) {
    if (mode_config.use_pio) {
        // PIO path: arbitrary pins
        hwuart_pio_fullduplex_init(
            mode_config.pio_tx_pin, mode_config.pio_rx_pin,
            mode_config.data_bits, mode_config.stop_bits,
            mode_config.parity, mode_config.baudrate);
        mode_config.baudrate_actual = mode_config.baudrate;
        pio_active = true;
        pio_active_tx_pin = mode_config.pio_tx_pin;
        pio_active_rx_pin = mode_config.pio_rx_pin;
        bio_buf_output(mode_config.pio_tx_pin);
        bio_buf_input(mode_config.pio_rx_pin);
        system_bio_update_purpose_and_label(true, mode_config.pio_tx_pin, BP_PIN_MODE, pin_labels[0]);
        system_bio_update_purpose_and_label(true, mode_config.pio_rx_pin, BP_PIN_MODE, pin_labels[1]);
        gpio_set_outover(bio2bufiopin[mode_config.pio_tx_pin],
                         mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        gpio_set_inover(bio2bufiopin[mode_config.pio_rx_pin],
                        mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
        // drain any glitch bytes from setup
        hwuart_pio_fullduplex_flush_rx();
        return 1;
    }

    // Hardware UART path
    mode_config.baudrate_actual = uart_init(M_UART_PORT, mode_config.baudrate);
    // setup peripheral
    uart_set_format(M_UART_PORT, mode_config.data_bits, mode_config.stop_bits, mode_config.parity);
    // set buffers to correct position
    bio_buf_output(M_UART_TX); // tx
    bio_buf_input(M_UART_RX);  // rx
    // assign peripheral to io pins
    bio_set_function(M_UART_TX, GPIO_FUNC_UART); // tx
    bio_set_function(M_UART_RX, GPIO_FUNC_UART); // rx
    system_bio_update_purpose_and_label(true, M_UART_TX, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_UART_RX, BP_PIN_MODE, pin_labels[1]);

    gpio_set_outover(bio2bufiopin[M_UART_TX], mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    gpio_set_inover(bio2bufiopin[M_UART_RX], mode_config.invert ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);


    if (!mode_config.flow_control) {
        uart_set_hw_flow(M_UART_PORT, false, false);
    } else {
        bio_set_function(M_UART_CTS, GPIO_FUNC_UART);
        bio_set_function(M_UART_RTS, GPIO_FUNC_SIO);
        bio_output(M_UART_RTS);
        // only show the pins if flow control is enabled in order to avoid confusion
        system_bio_update_purpose_and_label(true, M_UART_RTS, BP_PIN_MODE, pin_labels[2]);
        system_bio_update_purpose_and_label(true, M_UART_CTS, BP_PIN_MODE, pin_labels[3]);
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
    }

    // drain the buffer of any glitch bytes from setup
    while (uart_is_readable(M_UART_PORT)) {
        uart_getc(M_UART_PORT);
    }

    return 1;
}

bool hwuart_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 0x00);
}

void hwuart_periodic(void) {
    if (pio_active) {
        if (mode_config.async_print) {
            uint32_t raw;
            uint8_t cooked;
            if (hwuart_pio_fullduplex_read(&raw, &cooked)) {
                uint32_t val = (uint32_t)cooked;
                ui_format_print_number_2(&periodic_attributes, &val);
            }
        }
        return;
    }
    if (mode_config.async_print && uart_is_readable(M_UART_PORT)) {
        // printf("ASYNC: %d\r\n", uart_getc(M_UART_PORT));
        if(mode_config.flow_control) bio_put(M_UART_RTS, 0);
        uint32_t temp = uart_getc(M_UART_PORT);
        if(mode_config.flow_control) bio_put(M_UART_RTS, 1);
        ui_format_print_number_2(&periodic_attributes, &temp);
    }
}

void hwuart_open(struct _bytecode* result, struct _bytecode* next) {
    if (pio_active) {
        hwuart_pio_fullduplex_flush_rx();
    } else {
        // clear FIFO and enable UART
        if(mode_config.flow_control) bio_put(M_UART_RTS, 0);
        while (uart_is_readable(M_UART_PORT)) {
            uart_getc(M_UART_PORT);
        }
        if(mode_config.flow_control) bio_put(M_UART_RTS, 1);
    }

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
    if (pio_active) {
        hwuart_pio_fullduplex_write(result->out_data);
        return;
    }
    if (mode_config.blocking) {
        uart_putc_raw(M_UART_PORT, result->out_data);
    } else {
        uart_putc_raw(M_UART_PORT, result->out_data);
    }
}

void hwuart_read(struct _bytecode* result, struct _bytecode* next) {
    if (pio_active) {
        uint32_t timeout = 0xfff;
        uint32_t raw;
        uint8_t cooked;
        while (!hwuart_pio_fullduplex_read(&raw, &cooked)) {
            timeout--;
            if (!timeout) {
                result->error = SERR_ERROR;
                result->error_message = GET_T(T_UART_NO_DATA_READ);
                return;
            }
        }
        result->in_data = cooked;
        return;
    }

    uint32_t timeout = 0xfff;

    while (!uart_is_readable(M_UART_PORT)) {
        timeout--;
        if (!timeout) {
            result->error = SERR_ERROR;
            result->error_message = GET_T(T_UART_NO_DATA_READ);
            return;
        }
    }

    if(mode_config.flow_control) bio_put(M_UART_RTS, 0);
    if (uart_is_readable(M_UART_PORT)) {
        result->in_data = uart_getc(M_UART_PORT);
    } else {
        result->error = SERR_ERROR;
        result->error_message = GET_T(T_UART_NO_DATA_READ);
    }
    if(mode_config.flow_control) bio_put(M_UART_RTS, 1);
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
    if (pio_active) {
        hwuart_pio_deinit();
        system_bio_update_purpose_and_label(false, pio_active_tx_pin, BP_PIN_MODE, 0);
        system_bio_update_purpose_and_label(false, pio_active_rx_pin, BP_PIN_MODE, 0);
        gpio_set_outover(bio2bufiopin[pio_active_tx_pin], GPIO_OVERRIDE_NORMAL);
        gpio_set_inover(bio2bufiopin[pio_active_rx_pin], GPIO_OVERRIDE_NORMAL);
        pio_active = false;
        bio_init();
        return;
    }
    // disable peripheral
    uart_deinit(M_UART_PORT);
    system_bio_update_purpose_and_label(false, M_UART_TX, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UART_RX, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UART_RTS, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UART_CTS, BP_PIN_MODE, 0);
    gpio_set_outover(bio2bufiopin[M_UART_TX], GPIO_OVERRIDE_NORMAL);
    gpio_set_inover(bio2bufiopin[M_UART_RX], GPIO_OVERRIDE_NORMAL);
    gpio_set_inover(bio2bufiopin[M_UART_CTS], GPIO_OVERRIDE_NORMAL);
    gpio_set_outover(bio2bufiopin[M_UART_RTS], GPIO_OVERRIDE_NORMAL);
    uart_set_hw_flow(M_UART_PORT, false, false);
    // reset all pins to safe mode (done before mode change, but we do it here to be safe)
    bio_init();
}

void hwuart_settings(void) {
    ui_help_setting_int(GET_T(T_UART_SPEED_MENU), mode_config.baudrate, GET_T(T_UART_BAUD));
    ui_help_setting_int(GET_T(T_UART_DATA_BITS_MENU), mode_config.data_bits, 0x00);
    ui_help_setting_string(GET_T(T_UART_PARITY_MENU), GET_T(parity_choices[mode_config.parity].label), 0x00);
    ui_help_setting_int(GET_T(T_UART_STOP_BITS_MENU), mode_config.stop_bits, 0x00);
    ui_help_setting_string(GET_T(T_UART_FLOW_CONTROL_MENU),
            !mode_config.flow_control ? GET_T(T_UART_FLOW_CONTROL_MENU_1) : GET_T(T_UART_FLOW_CONTROL_MENU_2), 0x00);
    ui_help_setting_string(GET_T(T_UART_INVERT_MENU),
            !mode_config.invert ? GET_T(T_UART_INVERT_MENU_1) : GET_T(T_UART_INVERT_MENU_2), 0x00);
    if (mode_config.use_pio) {
        ui_help_setting_int("PIO TX pin", mode_config.pio_tx_pin, 0x00);
        ui_help_setting_int("PIO RX pin", mode_config.pio_rx_pin, 0x00);
    }
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
    if (pio_active) {
        return; // PIO writes block until the frame has been sent
    }
    uart_tx_wait_blocking(M_UART_PORT);
}

uint32_t hwuart_get_speed(void) {
    return mode_config.baudrate_actual;
}
