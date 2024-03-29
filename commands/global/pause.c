#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"

static const char * const usage[]={
    "pause",  
    "pause and wait for any key", 
};

void pause_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), 0x00,0x00)) return;
    printf("%s\r\n", t[T_PRESS_ANY_KEY]);
    char c;
    while(!rx_fifo_try_get(&c) && !system_config.error){
        busy_wait_ms(1);
    }   
}
