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
#include "pirate/bio.h"
#include "pirate/hwuart_pio.h"
#include "ui/ui_cmdln.h"

static const char * const usage[]= {
    "bridge\t[-h(elp)]",   
    "Transparent UART bridge: bridge",
    "Exit: press Bus Pirate button"
};

static const struct ui_help_options options[]= {
{1,"", T_HELP_UART_BRIDGE}, //command help
    {0, "-t", T_HELP_UART_BRIDGE_TOOLBAR},
    {0,"-h",T_HELP_FLAG}, //help
};


void hduart_bridge_handler(struct command_result *res){
    uint32_t raw;
	uint8_t cooked;

    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    if(!ui_help_check_vout_vref()) return;
    static const char label[]="RTS";

    bool toolbar_state= system_config.terminal_ansi_statusbar_pause;
    bool pause_toolbar=!cmdln_args_find_flag('t'|0x20);
    if(pause_toolbar){
        system_config.terminal_ansi_statusbar_pause = true;
    }

   	system_bio_claim(true, BIO2, BP_PIN_MODE, label);
    bio_output(BIO2);
    bio_put(BIO2, system_config.rts); 
    
    printf("%s%s%s\r\n", ui_term_color_notice(),t[T_HELP_UART_BRIDGE_EXIT], ui_term_color_reset());
    while(true){
        char c;
        bio_put(BIO2, !system_config.rts); 
        if(rx_fifo_try_get(&c)){
            hwuart_pio_write(c);
        }
        if(hwuart_pio_read(&raw, &cooked)){
            tx_fifo_put(&cooked);
        }
        //exit when button pressed.
        if(button_get(0)) break;   
    }

    bio_input(BIO2);
   	system_bio_claim(false, BIO2, BP_PIN_MODE,0);

    if(pause_toolbar){
        system_config.terminal_ansi_statusbar_pause = toolbar_state;
    }
}
