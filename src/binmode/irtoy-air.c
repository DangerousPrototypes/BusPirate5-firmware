#include <pico/stdlib.h>
#include <string.h>
#include "hardware/clocks.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"
// #include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and
// bytecode stuff to a single helper file #include "command_struct.h" //needed for same reason as bytecode and needs same fix
// #include "modes.h"
#include "binmode/binmodes.h"
#include "tusb.h"
#include "ui/ui_term.h"
#include "binmode/binio_helpers.h"


#define MAX_UART_PKT 64
#define CDC_INTF 1

// binmode name to display
const char irtoy_air_name[] = "AIR capture (AnalysIR, etc)";

// binmode setup on mode start
void irtoy_air_setup(void) {
    // time counter
    // learner counter
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

// binmode cleanup on exit
void irtoy_air_cleanup(void) {
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

#define HARDWARE_VERSION 3
#define FIRMWARE_VERSION_H '2'
#define FIRMWARE_VERSION_L '0'

// check the PIO fifo for new timing sequences
// end with timeout
void irtoy_air_rx(void){
    static bool active=false;
    char temp[10];
#if 0
    if(!active){
        //check FIFO for data, else just return
        if(!PIOFIFO()) return;

        //get learner frequency
        uint8_t cnt = snprintf(temp, count_of(temp), "$%d:", pio_output);
        script_send(temp, cnt); 
        active=true;
    }

    if(PIOFIFO()){
        //get the data
        if(pio_output>0){
            uint8_t cnt = snprintf(temp, count_of(temp), "%d,", pio_output);
            script_send(temp, cnt); 
        }else{
            //counter at 0, timeout
            bin_tx_fifo_put(';');
            active=false;
        }
    }     
#endif
        
}


/*
$36:420,280,168,280,168,616,168,448,168,448,168,280,168,280,168,280,168,280,168,616,168,616,168,448,168,616,168,280,168,280,168,280,168,448,168,90804,;
$ start character
: carrier frequency / 1000 (this comes from the learner sensor)
ASCII decimals representing the lengths of pulse and no-pulse in uS (anyone think that 90804 is a timeout?). CSV formatted, including the final value
; Terminated with ;
*/
// Use PIO to count 1uS ticks for each pulse and no-pulse, with timeout?
void irtoy_air_service(void){

    //need to be careful about PIO fifo overflow here
    if (!tud_cdc_n_connected(CDC_INTF)) {
        //rc5_drain_fifo();
        return;
    }
   
    //check the PIO for data in FIFO
    irtoy_air_rx();

    char c;
    if (!bin_rx_fifo_try_get(&c)){
        return;
    }

    switch (c) {
        case 'V':
        case 'v':// Acquire Version
            const char version[4] = {'V', (HARDWARE_VERSION + 0x30), FIRMWARE_VERSION_H, FIRMWARE_VERSION_L};
            script_send(version, 4);
            break;           
        default:
            break;

    }

}

