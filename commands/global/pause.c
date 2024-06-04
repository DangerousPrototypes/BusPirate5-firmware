#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "ui/ui_cmdln.h"
#include "pirate/button.h"

static const char * const usage[]={
    "pause and wait for any key: pause",  
    "pause and wait for button press: pause -b", 
};

void pause_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), 0x00,0x00)) return;

    //check for -b button flag
    bool pause_for_button=cmdln_args_find_flag('b'|0x20);
    if(pause_for_button){
        printf("%s\r\n", t[T_PRESS_BUTTON]);
        while(!button_get(0) && !system_config.error){busy_wait_ms(1);}
        return;
    }

    printf("%s\r\n", t[T_PRESS_ANY_KEY]);
    char c;
    while(!rx_fifo_try_get(&c) && !system_config.error){
        busy_wait_ms(1);
    }   
}
