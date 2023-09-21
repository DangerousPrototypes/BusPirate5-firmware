#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
//#include "buf.h"
#include "usb_tx.h"
#include "usb_rx.h"

void core1_entry(void) 
{ 
    char c;
    /*uint32_t delay_ms;
    uint8_t mode=1;

    while (1){
      if(multicore_fifo_rvalid())
      {
        mode=multicore_fifo_pop_blocking();
      }
      delay_ms=rgb_update(mode);
      busy_wait_ms(delay_ms);
    }
    */
    tx_fifo_init();
    rx_fifo_init();
    while(1)
    {

        //TODO: interupt and service commmands attached to SPI like pullups, amux, sd card, lcd
        tx_fifo_service();
    }

}

