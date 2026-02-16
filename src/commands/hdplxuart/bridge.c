#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/bio.h"
#include "pirate/hwuart_pio.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const usage[] = { "bridge\t[-h(elp)]",
                                     "Transparent UART bridge:%s bridge",
                                     "Exit:%s press Bus Pirate button" };

static const bp_command_opt_t hduart_bridge_opts[] = {
    { "toolbar",  't', BP_ARG_NONE, NULL, T_HELP_UART_BRIDGE_TOOLBAR },
    { "suppress", 's', BP_ARG_NONE, NULL, T_HELP_UART_BRIDGE_SUPPRESS_LOCAL_ECHO },
    { 0 }
};

const bp_command_def_t hduart_bridge_def = {
    .name         = "bridge",
    .description  = T_HELP_UART_BRIDGE,
    .actions      = NULL,
    .action_count = 0,
    .opts         = hduart_bridge_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void hduart_bridge_handler(struct command_result* res) {
    uint32_t raw;
    uint8_t cooked;

    if (bp_cmd_help_check(&hduart_bridge_def, res->help_flag)) {
        return;
    }
    if (!ui_help_check_vout_vref()) {
        return;
    }
    static const char label[] = "RTS";

    bool toolbar_state = system_config.terminal_ansi_statusbar_pause;
    bool pause_toolbar = !bp_cmd_find_flag(&hduart_bridge_def, 't');
    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = true;
    }

    bool suppress_local_echo = bp_cmd_find_flag(&hduart_bridge_def, 's');

    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, label);
    bio_output(BIO2);
    bio_put(BIO2, system_config.rts);

    printf("%s%s%s\r\n", ui_term_color_notice(), GET_T(T_HELP_UART_BRIDGE_EXIT), ui_term_color_reset());
    while (true) {
        char c;
        bio_put(BIO2, !system_config.rts);
        if (rx_fifo_try_get(&c)) {
            hwuart_pio_write(c);
            if (!suppress_local_echo) {
                tx_fifo_put(&c);
            }
        }
        if (hwuart_pio_read(&raw, &cooked)) {
            char c = (char)cooked;
            tx_fifo_put(&c);
        }
        // exit when button pressed.
        if (button_get(0)) {
            break;
        }
    }

    bio_input(BIO2);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);

    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = toolbar_state;
    }
}
