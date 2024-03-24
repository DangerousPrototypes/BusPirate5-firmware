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
#include "hwuart.pio.h"
#include "pirate/bio.h"

static const char * const usage[]= {
    "sim\t[-h(elp)]",   
};

static const struct ui_help_options options[]= {
{1,"", T_HELP_UART_BRIDGE}, //command help
    {0,"-h",T_HELP_FLAG}, //help
};


static PIO pio;
static uint sm;
static uint pio_loaded_offset;

void simcard_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    if(!ui_help_check_vout_vref()) return;

    pio_loaded_offset = pio_add_program(M_I2C_PIO, &uart_rx_program);
    uart_rx_program_init(M_I2C_PIO, M_I2C_PIO_SM, pio_loaded_offset, bio2bufiopin[BIO0], 9600);

    pio_remove_program(M_I2C_PIO, &uart_rx_program, pio_loaded_offset);
}