#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"
#include "opt_args.h" // File system related
#include "fatfs/ff.h" // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h" // This file is needed for the command line parsing functions
#include "ui/ui_term.h"    // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "pirate/amux.h"
#include "hardware/pwm.h"
#include "pirate/bio.h"

static const char * const usage[]= {

};

static const struct ui_help_options options[]= {
{1,"", T_HELP_BUTTON}, 
    {0,"short",T_HELP_BUTTON_SHORT}, 
    {0,"long",T_HELP_BUTTON_LONG}, 
    {0,"-f",T_HELP_BUTTON_FILE}, 
    {0,"-d",T_HELP_BUTTON_HIDE}, 
    {0,"-e",T_HELP_BUTTON_EXIT}, 
    {0,"-h",T_HELP_FLAG}, 
};

void smps_handler(struct command_result *res){
    //check help
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;

    //disable analog subsystem

    //measure voltage
    uint32_t raw = amux_read_bio(BIO2);
    #define PWM_TOP 14000 //0x30D3
    //PWM setup
   //Current adjust is slice 4 channel a
    //voltage adjust is slice 4 channel b
    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[BIO3]);
    uint chan_num = pwm_gpio_to_channel(bio2bufiopin[BIO3]);    
    //10KHz clock, into our 1K + 0.1uF filter
    pwm_set_clkdiv_int_frac(slice_num, 16>>4,16&0b1111);
    pwm_set_wrap(slice_num,PWM_TOP);
    pwm_set_chan_level(slice_num, chan_num, (uint16_t)(PWM_TOP/2));
    bio_output(BIO3);
    //enable output
    gpio_set_function(bio2bufiopin[BIO3], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);        

    //@7 volts divider output = 2.55
    //2.55=((6600*hw_adc_raw[X])/4096);
    //2.55*4096/6600=hw_adc_raw[X]
    //hw_adc_raw[X]=2.55*4096/6600
    //1582

    #define TARGET 1582
    bool pwm_on = true;
    while(true){
        //measure voltage
        raw = amux_read_bio(BIO2);
        //adjust PWM
        if(raw>TARGET){
            //PWM off
            pwm_set_enabled(slice_num, false); 
            pwm_on = false;
        }else if(raw<TARGET && !pwm_on){
            //PWM on
            pwm_set_enabled(slice_num, true); 
            pwm_on = true;
        }
        busy_wait_us(500);
    }
 


    //loop: measure voltage, adjust PWM
 
}


