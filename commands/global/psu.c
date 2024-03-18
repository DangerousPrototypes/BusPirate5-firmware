#include <stdio.h>
#include "pico/stdlib.h"
//#include "hardware/clocks.h"
#include "pirate.h"
#include "opt_args.h"
//#include "hardware/timer.h"
//#include "shift.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "system_monitor.h"
#include "commands/global/psu.h"
//#include "amux.h"
#include "display/scope.h"
#include "ui/ui_cmdln.h"
#include "pirate/psu.h"

// current limit fuse tripped
void psu_irq_callback(void){
    psu_reset(); //also sets system_config.psu=0
    system_config.psu_irq_en=false;
    system_config.psu_current_error=true;
    system_config.psu_error=true;
    system_config.error=true;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[5]);
}

void psu_enable(struct command_result *res){
    float volts,current;
    bool current_limit_override=false;

    //todo: add to system config?
    if (scope_running) { // scope is using the analog subsystem
	    printf("Can't turn the power supply on when the scope is using the analog subsystem - use the 'ss' command to stop the scope\r\n");
	    return;
    }

    system_config.psu=0;
    system_config.pin_labels[0]=0;
    system_config.pin_changed=0xff;
    system_pin_claim(false, BP_VOUT, 0, 0);

    bool has_volts = cmdln_args_float_by_position(1, &volts);
    bool has_current = cmdln_args_float_by_position(2, &current);
    if(has_volts && !has_current)
    {
        current_limit_override=true;
    }

    if(!has_volts){
        //prompt voltage (float)
        printf("%sPower supply\r\nVolts (0.80V-5.00V)%s", ui_term_color_info(), ui_term_color_reset());  
        prompt_result result;
        ui_prompt_float(&result, 0.8f, 5.0f, 3.3f, true, &volts, false);
        if(result.exit)
        {
            res->error=true;
            return;
        }

        //prompt current (float)
        printf("%sMaximum current (0mA-500mA), <enter> for none%s", ui_term_color_info(), ui_term_color_reset());
        prompt_result result;
        ui_prompt_float(&result, 0.0f, 500.0f, 100.0f, true, &current, true);
        if(result.exit)
        {
            res->error=true;
            return;    
        }
        if(result.default_value) //enter for none...
        {     
            current_limit_override=true;
        }   
    }

    psu_enable(volts, current, current_limit_override);

    //get: actual voltage, actual current, startup code, measure current
#if 0
    //todo: use return voltage
    system_config.psu_voltage=((vset*psu_v_per_bit)+(PSU_V_LOW*10));
    //printf("vset: %d, psu_v_per_bit: %d", vset,psu_v_per_bit);
    // actual voltage
    float vact=(float)((float)((vset*psu_v_per_bit)+(PSU_V_LOW*10))/(float)10000);
    printf("%s%1.2f%sV%s requested, closest value: %s%1.2f%sV\r\n", 
        ui_term_color_num_float(), volts, ui_term_color_reset(), ui_term_color_info(),
        ui_term_color_num_float(), vact, ui_term_color_reset()    
    );
    system_config.psu_dac_v_set=vset;

    if(!result.default_value) //enter for none...
    {
        system_config.psu_current_limit_en=true;
        system_config.psu_dac_i_set=(uint32_t)iset;
        system_config.psu_current_limit=(uint32_t)((float)iset*(float)psu_i_per_bit);
        printf("%s%1.1f%smA%s requested, closest value: %s%3.1f%smA\r\n", 
            ui_term_color_num_float(), current, ui_term_color_reset(), ui_term_color_info(),
            ui_term_color_num_float(), iact, ui_term_color_reset()    
        );
    }
    else
    {
        system_config.psu_current_limit_en=false;
        printf("%s%s:%s%s\r\n",
        ui_term_color_notice(),
        t[T_INFO_CURRENT_LIMIT],
        ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );
    }    

    system_config.psu_error=true;

    
    //todo: consistent interface to each label of toolbar and LCD, including vref/vout
    system_config.psu=1;
    system_config.psu_error=false;
    system_config.psu_current_error=false;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
    monitor_clear_current(); //reset current so the LCD gets all characters
    
    printf("\r\n%s%s:%s%s\r\n",
        ui_term_color_notice(),
        t[T_MODE_POWER_SUPPLY],
        ui_term_color_reset(),
        t[T_MODE_ENABLED]
    );
    
    // print voltage and current、、TODO：get current from psu library
    uint32_t isense=((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000)/4095));
    printf("%sVreg output: %s%d.%d%sV%s, Vref/Vout pin: %s%d.%d%sV%s, Current sense: %s%d.%d%smA%s\r\n%s", 
    ui_term_color_notice(), 
    ui_term_color_num_float(), ((hw_adc_voltage[HW_ADC_MUX_VREG_OUT])/1000), (((hw_adc_voltage[HW_ADC_MUX_VREG_OUT])%1000)/100), ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_num_float(), ((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])/1000), (((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])%1000)/100), ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_num_float(), (isense/1000), ((isense%1000)/100),ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_reset()
    );

    //gpio_set_irq_enabled_with_callback(CURRENT_DETECT, 0b0001, true, &psu_irq_callback);
    //since we dont have any more pins, the over current detect system is read through the 
    //4067 and ADC. It will be picked up in the second core loop
    if(system_config.psu_current_limit_en)
    {
        system_config.psu_irq_en=true;
    }
    #endif
    return;
}

//cleanup on mode exit, etc
void psucmd_cleanup(void){
    psu_disable();
    system_config.psu_error=false;
    system_config.psu=0;
    system_config.info_bar_changed=true;
    monitor_clear_current(); //reset current so the LCD gets all characters next time
}

void psucmd_disable(struct command_result *res){
    psu_disable();
    printf("%s%s: %s%s\r\n",
        ui_term_color_notice(),
        t[T_MODE_POWER_SUPPLY],
        ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );        
    psu_cleanup();
}

bool psucmd_init(void){       
    system_config.psu=0;
    system_config.psu_error=true;
    psu_init();
    system_config.psu_error=false;
    return true;
}

