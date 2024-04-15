#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
#include "usb_rx.h"
#include "usb_tx.h"

static const char * const usage[]= {
    "bridge\t[-h(elp)]",   
    "Transparent UART bridge: bridge",
    "Exit: press Bus Pirate button"
};

static const struct ui_help_options options[]= {
{1,"", T_HELP_UART_BRIDGE}, //command help
    {0,"-h",T_HELP_FLAG}, //help
};


void uart_bridge_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    if(!ui_help_check_vout_vref()) return;
    
    printf("%sUART bridge. Press Bus Pirate button to exit.%s\r\n", ui_term_color_notice(), ui_term_color_reset());
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
        if(button_get(0)) return;   
    }
}
