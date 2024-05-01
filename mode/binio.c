/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Binary access modes for Bus Pirate scripting */

//#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "pirate.h"
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "commands.h"
#include "modes.h"
#include "mode/binio.h"
#include "pirate/pullup.h"
#include "pirate/psu.h"
#include "pirate/amux.h"
#include "sump.h"
#include "binio_helpers.h"
#include "tusb.h"

unsigned char binBBpindirectionset(unsigned char inByte);
unsigned char binBBpinset(unsigned char inByte);
void binBBversion(void);
void binSelfTest(unsigned char jumperTest);
void binReset(void);
unsigned char getRXbyte(void);

void binBBversion(void){
    //const char version_string[]="BBIO1";
    script_print("BBIO2");
}

void script_enabled(void){
    printf("\r\nScripting mode enabled. Terminal locked.\r\n");
}

void script_disabled(void){
    printf("\r\nTerminal unlocked.\r\n");     //fall through to prompt 
}

//0x00 reset
//w/W Power supply (off/ON)
//p/P Pull-up resistors (off/ON)
//a/A/@ x Set IO x state (low/HI/READ)
//v x/V x Show volts on IOx (once/CONT)
enum {
    BM_RESET = 0,
    BM_POWER,
    BM_PULLUP,
    BM_AUX,
    BM_ADC,
};

//functions for global binmode commands
// maybe splitting global commands from mode commands 0-127, 127-255 might speed processing?
typedef struct _global_binmode{
	void (*global_reset)(void);			// reset
    void (*global_power)(void);			// power
    void (*global_pullup)(void);		// pullup
    void (*global_aux)(void);			// aux
    void (*global_adc)(void);			// adc
} _global_binmode;

// handler needs to be cooperative multitasking until mode is enabled
bool script_mode(void){
    //could activate binmode just by opening the port?
    //if(!tud_cdc_n_connected(1)) return false;  
    //if(!tud_cdc_n_available(1)) return false;
    //script_enabled();
    //while(true){
        //do an echo test so we can interface via a mode ;)
        //if(tud_cdc_n_available(1)){
            char c;
            if(bin_rx_fifo_try_get(&c)) bin_tx_fifo_put(c);
        //}

    //}
    return false;
}

 
 



