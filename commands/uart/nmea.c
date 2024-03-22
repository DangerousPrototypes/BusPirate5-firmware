#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "lib/minmea/gps.h"
#include "lib/minmea/minmea.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "usb_rx.h"
#include "usb_tx.h"

static const char * const usage[]= {
    "gps\t[-h(elp)]",   
    "Decode GPS NMEA packets: gps",
    "Exit: press any key"
};

static const struct ui_help_options options[]= {
{1,"", T_HELP_UART_NMEA}, //command help
    {0,"-h",T_HELP_FLAG}, //help
};


void nmea_decode_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    if(!ui_help_check_vout_vref()) return;

    printf("%s%s%s\r\n%s", ui_term_color_notice(), t[T_PRESS_ANY_KEY_TO_EXIT], ui_term_color_reset(), ui_term_cursor_hide());
    
    while (true){
        char line[MINMEA_MAX_SENTENCE_LENGTH];
        uint32_t nmea_cnt=0;

        while(true){
            char c;
            if(rx_fifo_try_get(&c)) return;

            if(uart_is_readable(M_UART_PORT)){
                uint32_t temp = uart_getc(M_UART_PORT);
                if(nmea_cnt>0 || temp=='$'){
                    line[nmea_cnt]=temp;
                    nmea_cnt++;
                    if(nmea_cnt>=MINMEA_MAX_SENTENCE_LENGTH){
                        nmea_cnt=0;
                    }

                    if(temp==0x0a){
                        line[nmea_cnt]=0x00;
                        break;
                    } 
                }
            }
        }
        printf(line);
        process_gps(line);
    }
}
