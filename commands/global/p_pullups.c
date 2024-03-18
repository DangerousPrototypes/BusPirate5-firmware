#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "shift.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "amux.h"

const char * const p_usage[]={
    "p|P",  
    "Disable: p", 
    "Enable: P",
};

const struct ui_help_options p_help[]={
{1,"", T_HELP_GCMD_P}, //command help
    {0,"w",T_CONFIG_DISABLE }, 
    {0,"W",T_CONFIG_ENABLE }, 
};


static bool show_help(bool help_flag){
    if(help_flag){
        ui_help_usage(p_usage, count_of(p_usage));
        ui_help_options(&p_help[0],count_of(p_help));
        return true;
    }
    return false;
}

void pullups_enable_exc(void){
    HW_BIO_PULLUP_ENABLE();   
    system_config.pullup_enabled=1; 
    system_config.info_bar_changed=true;
}

void pullups_enable(struct command_result *res){
    //check help
	if(show_help(res->help_flag)) return;
		
    pullups_enable_exc();
    
    amux_sweep();
    
    printf("%s%s:%s %s (%s @ %s%d.%d%sV)", 
        ui_term_color_notice(), t[T_MODE_PULLUP_RESISTORS],	ui_term_color_reset(), 
        t[T_MODE_ENABLED], BP_HARDWARE_PULLUP_VALUE, 
        ui_term_color_num_float(), hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]/1000, 
        (hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]%1000)/100, ui_term_color_reset()
    );

    //TODO test outside debug mode
    if(hw_adc_raw[HW_ADC_MUX_VREF_VOUT]<250)//arbitrary
    {
        printf("\r\nWarning: no/low voltage detected. Enable power supply (W) or attached external supply to Vout/Vref");
    }
}

void pullups_cleanup(void){
    HW_BIO_PULLUP_DISABLE();   
	system_config.pullup_enabled=0;
    system_config.info_bar_changed=true;
}

void pullups_disable(struct command_result *res){
    if(show_help(res->help_flag)) return;

    pullups_cleanup();

    printf("%s%s:%s %s",
        ui_term_color_notice(),t[T_MODE_PULLUP_RESISTORS],ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );
}
