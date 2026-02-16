#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "lib/bp_args/bp_cmd.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const usage[] = { "bridge\t[-h(elp)] [-t(oolbar)]",
                                     "Transparent UART bridge:%s bridge",
                                     "Exit:%s press Bus Pirate button" };

static const bp_command_opt_t bridge_opts[] = {
    { "toolbar", 't', BP_ARG_NONE, NULL, T_HELP_UART_BRIDGE_TOOLBAR },
    { 0 }
};

const bp_command_def_t uart_bridge_def = {
    .name = "bridge",
    .description = T_HELP_UART_BRIDGE,
    .actions = NULL,
    .action_count = 0,
    .opts = bridge_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};

void uart_bridge_handler(struct command_result* res) {
    if (bp_cmd_help_check(&uart_bridge_def, res->help_flag)) {
        return;
    }
    if (!ui_help_check_vout_vref()) {
        return;
    }

    bool toolbar_state = system_config.terminal_ansi_statusbar_pause;
    bool pause_toolbar = !bp_cmd_find_flag(&uart_bridge_def, 't');
    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = true;
    }

    printf("%s%s%s\r\n", ui_term_color_notice(), GET_T(T_HELP_UART_BRIDGE_EXIT), ui_term_color_reset());
    bio_put(M_UART_RTS, 0);
    while (true) {
        char c;
        if (rx_fifo_try_get(&c)) {
            uart_putc_raw(M_UART_PORT, c);
        }
        if (uart_is_readable(M_UART_PORT)) {
            c = uart_getc(M_UART_PORT);
            tx_fifo_put(&c);
        }
        // exit when button pressed.
        if (button_get(0)) {
            break;
        }
    }
    bio_put(M_UART_RTS, 1);

    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = toolbar_state;
    }
}
