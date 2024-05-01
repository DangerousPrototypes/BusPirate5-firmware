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


void script_print(const char *str) {
    for(uint8_t i=0; i<strlen(str); i++){
        bin_tx_fifo_put(str[i]);
    }
}

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

#define BINMODE_COMMAND 0
#define BINMODE_ARG 1
#define BINMODE_MAX_ARGS 4

//0x00 reset
//w/W Power supply (off/ON)
//p/P Pull-up resistors (off/ON)
//a/A/@ x Set IO x state (low/HI/READ)
//v x/V x Show volts on IOx (once/CONT)
// maybe splitting global commands from mode commands 0-127, 127-255 might speed processing?
enum {
    BM_RESET = 0,
    BM_POWER_EN,
    BM_POWER_DIS,
    BM_PULLUP_EN,
    BM_PULLUP_DIS,
    BM_LIST_MODES,
    BM_CHANGE_MODE,
    BM_INFO,
    BM_VERSION_HW,
    BM_VERSION_FW,
    BM_BITORDER_MSB,
    BM_BITORDER_LSB,
    BM_AUX_DIRECTION_MASK,
    BM_AUX_READ,
    BM_AUX_HIGH_MASK,
    BM_AUX_LOW_MASK,
    BM_ADC,
    BM_ADC_DIVIDER,
    BM_ADC_RAW,
    BM_PWM_EN,
    BM_PWM_DIS,
    BM_PWM_RAW,
    BM_FREQ,
    BM_FREQ_RAW,
    BM_BOOTLOADER,
    BM_RESET_BUSPIRATE,
    BM_PRINT_STRING,
};

static uint8_t binmode_args[BINMODE_MAX_ARGS];

struct _binmode_global_struct{
    void (*func)(void);
    uint32_t arg_count;
};

// [/{ Start/Start II (mode dependent) 	]/} Stop/Stop II (mode dependent)
// 123 Write value (decimal) 	r Read
// / Clock high 	\ Clock low
// ^ Clock tick 	- Data high
// _ Data low 	. Read data pin state
// d/D Delay 1 us/MS (d:4 to repeat) 	a/A/@.x Set IO.x state (low/HI/READ)
// v.x Measure volts on IO.x 	> Run bus syntax (really a global command…)
enum{
    BM_WRITE=0,
    BM_START,
    BM_START_ALT,
    BM_STOP,
    BM_STOP_ALT,
    BM_READ,
    BM_CLKH,
    BM_CLKL,
    BM_TICK,
    BM_DATH,
    BM_DATL,
    BM_BITR,
    BM_DELAY,
    BM_DELAY_MS
};

struct _binmode_local_struct{
    void (*func)(void);
    uint32_t arg_count;
};

void binmode_write(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_write(&result, &next);
}

void binmode_start(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_start(&result, &next);
}

void binmode_start_alt(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_start_alt(&result, &next);
}

void binmode_stop(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_stop(&result, &next);
}

void binmode_stop_alt(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_stop_alt(&result, &next);
}

void binmode_read(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_read(&result, &next);
}

void binmode_clkh(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_clkh(&result, &next);
}

void binmode_clkl(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_clkl(&result, &next);
}

void binmode_tick(void){
    struct _bytecode result; 
    struct _bytecode next;
    //modes[system_config.mode].protocol_tick(&result, &next);
}

void binmode_dath(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_dath(&result, &next);
}

void binmode_datl(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_datl(&result, &next);
}

void binmode_bitr(void){
    struct _bytecode result; 
    struct _bytecode next;
    modes[system_config.mode].protocol_bitr(&result, &next);
}

void binmode_delay(void){
    struct _bytecode result; 
    struct _bytecode next;
    //modes[system_config.mode].protocol_delay(&result, &next);
}

void binmode_delay_ms(void){
    struct _bytecode result; 
    struct _bytecode next;
    //modes[system_config.mode].protocol_delay_ms(&result, &next);
}

static const struct _binmode_local_struct local_commands[]={ 
    [BM_WRITE]={&binmode_write,1},
    [BM_START]={&binmode_start,0},
    [BM_START_ALT]={&binmode_start_alt,0},
    [BM_STOP]={&binmode_stop,0},
    [BM_STOP_ALT]={&binmode_stop_alt,0},
    [BM_READ]={&binmode_read,0},
    [BM_CLKH]={&binmode_clkh,0},
    [BM_CLKL]={&binmode_clkl,0},
    [BM_TICK]={&binmode_tick,0},
    [BM_DATH]={&binmode_dath,0},
    [BM_DATL]={&binmode_datl,0},
    [BM_BITR]={&binmode_bitr,0},
    [BM_DELAY]={&binmode_delay,1},
    [BM_DELAY_MS]={&binmode_delay_ms,1},
};


void binmode_reset(void){
    return;
}

void binmode_psu_enable(void){
    if(binmode_args[0]>5) return;
    if(binmode_args[1]>99) return;
    float voltage=binmode_args[0]+binmode_args[1]/100.0;
    float current=binmode_args[2];
    psu_enable(voltage,current, binmode_args[3]?true:false);
}

void mode_list(void){
    for(uint8_t i=0;i<count_of(modes);i++){
        script_print(modes[i].protocol_name);
        bin_tx_fifo_put(';');
    }
}

// TODO: capture arguments for mode name until 0x00
// TODO: compare mode string to modes.protocol_name
// TODO: success (protocol name) or fail (unknown mode)
void mode_change(void){
    if(binmode_args[0]>count_of(modes)) return;
    modes[system_config.mode].protocol_cleanup();
    system_config.mode=binmode_args[0];
    modes[system_config.mode].protocol_setup();
}

static const struct _binmode_global_struct global_commands[]={ 
    [BM_RESET]={&binmode_reset,0},
    [BM_POWER_EN]={&binmode_psu_enable,4},
    [BM_POWER_DIS]={&psu_disable,0},
    [BM_PULLUP_EN]={&pullup_enable,0},
    [BM_PULLUP_DIS]={&pullup_disable,0},
    [BM_LIST_MODES]={&mode_list,0},
    [BM_CHANGE_MODE]={&mode_change,1},
    //[BM_INFO]={&i_info_handler},
    //[BM_AUX]={&auxio_input_handler},
    //[BM_ADC]={&adc_measure_single},
};

// NOTE: THIS DOES NOT WORK ATM BECAUSE I TRIED TO GET CUTE AND BROKE LOTS OF STUFF
// NOTE2: I'M PRETTY UNHAPPY WITH REPEATING THE TESTS FOR GLOBAL/LOCAL, BUT I WANT THAT SEPARATION.
// handler needs to be cooperative multitasking until mode is enabled
bool script_mode(void){
    static uint8_t binmode_state=BINMODE_COMMAND;
    static uint8_t binmode_command;

    static uint8_t binmode_arg_count=0;
    //could activate binmode just by opening the port?
    //if(!tud_cdc_n_connected(1)) return false;  
    //if(!tud_cdc_n_available(1)) return false;
    //script_enabled();
    //while(true){
        //do an echo test so we can interface via a mode ;)
        //if(tud_cdc_n_available(1)){
            char c;
            if(bin_rx_fifo_try_get(&c)){
                //bin_tx_fifo_put(c); //echo for debug   
        
                switch(binmode_state){
                    case BINMODE_COMMAND:
                        if(c<0xf7 && c<count_of(global_commands)){
                            if(global_commands[c].arg_count>0){
                                binmode_command=c;
                                binmode_arg_count=global_commands[c].arg_count;
                                binmode_state=BINMODE_ARG;
                                break; //need more args
                            }
                            global_commands[c].func();
                        }
                        if(c>0xf7 && c<count_of(local_commands)){
                            c; //clear upper
                            if(local_commands[c&=0b10000000].arg_count>0){
                                binmode_command=c;
                                binmode_arg_count=local_commands[c&=0b10000000].arg_count;
                                binmode_state=BINMODE_ARG;
                                break; //need more args
                            }
                            local_commands[c&=0b10000000].func();
                        }
                        break; //invalid command
                    case BINMODE_ARG:
                        binmode_args[binmode_arg_count]=c;
                        binmode_arg_count++;
                        if(binmode_arg_count>=global_commands[binmode_command].arg_count){
                            global_commands[binmode_command].func();
                            binmode_state=BINMODE_COMMAND;
                            binmode_arg_count=0;
                        }
                        break;

                }
            
            }
        //}

    //}
    return false;
}

 
 



