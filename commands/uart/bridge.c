#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_cmdln.h"

static const char * const usage[]= {
    "bridge\t[-h(elp)] [-t(oolbar)]",
    "Transparent UART bridge: bridge",
    "Exit: press Bus Pirate button"
};

static const struct ui_help_options options[]= {
{1,"", T_HELP_UART_BRIDGE}, //command help
    {0, "-t", T_HELP_UART_BRIDGE_TOOLBAR},
    {0,"-h",T_HELP_FLAG}, //help
};


void uart_bridge_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    if(!ui_help_check_vout_vref()) return;

    bool toolbar_state= system_config.terminal_ansi_statusbar_pause;
    bool pause_toolbar=!cmdln_args_find_flag('t'|0x20);
    if(pause_toolbar){
        system_config.terminal_ansi_statusbar_pause = true;
    }

    printf("%s%s%s\r\n", ui_term_color_notice(), t[T_HELP_UART_BRIDGE_EXIT], ui_term_color_reset());
    bio_put(M_UART_RTS, 0);
    while(true){
        char c;
        if(rx_fifo_try_get(&c)){
            uart_putc_raw(M_UART_PORT, c);
        }
        if(uart_is_readable(M_UART_PORT)){
            c=uart_getc(M_UART_PORT);
            tx_fifo_put(&c);
        }
        //exit when button pressed.
        if(button_get(0)) break;
    }
    bio_put(M_UART_RTS, 1);

    if(pause_toolbar){
        system_config.terminal_ansi_statusbar_pause = toolbar_state;
    }
}
