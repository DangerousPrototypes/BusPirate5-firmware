#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"

#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "amux.h"
#include "pirate/pullup.h"

const char * const p_usage[]={
    "p|P",  
    "Disable: p", 
    "Enable: P",
};

const struct ui_help_options p_options[]={
{1,"", T_HELP_GCMD_P}, //command help
    {0,"p",T_CONFIG_DISABLE }, 
    {0,"P",T_CONFIG_ENABLE }, 
};

void pullups_enable(void){
    system_config.pullup_enabled=1; 
    system_config.info_bar_changed=true;
    pullup_enable();
}

void pullups_enable_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,p_usage,count_of(p_usage), &p_options[0],count_of(p_options) )) return;
		
    pullups_enable();
    
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
        printf("\r\nWarning: no/low voltage detected.\r\nEnable power supply (W) or attach external supply to Vout/Vref");
    }
}

void pullups_disable(void){
    system_config.pullup_enabled=0;
    system_config.info_bar_changed=true;
    pullup_disable();
}

void pullups_disable_handler(struct command_result *res){
    if(ui_help_show(res->help_flag,p_usage,count_of(p_usage), &p_options[0],count_of(p_options) )) return;

    pullups_disable();

    printf("%s%s:%s %s",
        ui_term_color_notice(),t[T_MODE_PULLUP_RESISTORS],ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );
}

void pullups_init(void){
    pullup_init();
}
