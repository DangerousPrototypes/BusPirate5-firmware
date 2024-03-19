#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "usb_rx.h"

void pause_any_key(void){
    char c;
    while(!rx_fifo_try_get(&c) && !system_config.error){
        busy_wait_ms(1);
    }   
}
