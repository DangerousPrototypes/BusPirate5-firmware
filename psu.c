#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "opt_args.h"
#include "hardware/timer.h"
#include "shift.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "system_monitor.h"
#include "psu.h"
#include "amux.h"

#define PWM_TOP 14000 //0x30D3

// voltage settings
#define PSU_V_LOW 800  //millivolts
#define PSU_V_HIGH 5000 //millivolts
#define PSU_V_RANGE ((PSU_V_HIGH*10) - (PSU_V_LOW*10))
 
// current settings
#define PSU_I_LOW 0 //mA
#define PSU_I_HIGH 500 //mA
#define PSU_I_RANGE ((PSU_I_HIGH*10000) - (PSU_I_LOW*10000))

static void psu_fuse_reset(){
    //reset current trigger
    shift_set_clear_wait(0, CURRENT_RESET); //low to activate the pnp
    busy_wait_ms(1);
    shift_set_clear_wait(CURRENT_RESET, 0); //high to disable  
}

static void psu_vreg_enable(bool enable){
    if(enable)
    {
        shift_set_clear_wait(0,CURRENT_EN); //low is on (PNP)
    }
    else
    {
        shift_set_clear_wait(CURRENT_EN,0); //high is off
    }
}

bool psu_reset(void)
{
    psu_vreg_enable(false); 
    //Current adjust is slice 4 channel a
    //voltage adjust is slice 4 channel b
    uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
    uint v_chan_num= pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
    uint i_chan_num= pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);  
    // first we start with full current because the inrush will often trip the fuse on low limits
    pwm_set_chan_level(slice_num, v_chan_num, PWM_TOP);
    pwm_set_chan_level(slice_num, i_chan_num, 0);       
    
    shift_set_clear_wait(0, CURRENT_EN_OVERRIDE); 
    
    busy_wait_ms(1);
    psu_fuse_reset(); //reset fuse so it isn't draining current from the opamp
    system_config.psu=0;
    system_config.psu_irq_en=false;
    system_pin_claim(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[0]); //change back to vref type pin
    return true;  
} 

// current limit fuse tripped
//void psu_irq_callback(uint gpio, uint32_t events)
void psu_irq_callback(void)
{
    psu_reset(); //also sets system_config.psu=0
    system_config.psu_irq_en=false;
    system_config.psu_current_error=true;
    system_config.psu_error=true;
    system_config.error=true;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[5]);
    //gpio_set_irq_enabled(gpio, events, false);
    //gpio_acknowledge_irq(gpio, events);   
}


uint32_t psu_set(float volts, float current, bool fuse_en)
{
    system_config.psu=0;
    system_config.pin_labels[0]=0;
    system_config.pin_changed=0xff;
    system_pin_claim(false, BP_VOUT, 0, 0);

    uint32_t psu_v_per_bit=((PSU_V_RANGE)/PWM_TOP);
    uint32_t vset=(uint32_t)((float)volts * 10000);
    vset=vset-(PSU_V_LOW*10);
    vset=vset/psu_v_per_bit;
    if(vset>PWM_TOP) vset=PWM_TOP+1;

    system_config.psu_voltage=((vset*psu_v_per_bit)+(PSU_V_LOW*10));

    // inverse for VREG margining
    vset=(PWM_TOP-vset);
    system_config.psu_dac_v_set=vset;

    float iset=(float)PWM_TOP;
    float psu_i_per_bit= ((float)(PSU_I_RANGE)/(float)PWM_TOP);
    iset= (float)((float)current * 10000);
    iset/=psu_i_per_bit;
    system_config.psu_dac_i_set=(uint32_t)iset;
    system_config.psu_current_limit=(uint32_t)((float)iset*(float)psu_i_per_bit);
    float iact=(float)((float)iset*psu_i_per_bit)/10000;

    system_config.psu_error=true;

    //Current adjust is slice 4 channel a
    //voltage adjust is slice 4 channel b
    uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
    uint v_chan_num= pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
    uint i_chan_num= pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);  
    // first we start with full current because the inrush will often trip the fuse on low limits
    pwm_set_chan_level(slice_num, v_chan_num, (uint16_t)vset);
    pwm_set_chan_level(slice_num, i_chan_num, PWM_TOP);    
    
    if(!fuse_en)
    { 
        shift_set_clear_wait(CURRENT_EN_OVERRIDE,0);
    }

    psu_fuse_reset();
    psu_vreg_enable(true);
    busy_wait_ms(10);

    //after a delay for inrush, we set the actual limit
    pwm_set_chan_level(slice_num, i_chan_num, (uint16_t)iset);  
    busy_wait_ms(500);
    
    amux_sweep();

    // did the fuse blow?
    // error,  close everything down
    if(fuse_en)
    {
        if( hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT] < 300 )
        {
            psu_reset();
            return 3; 
        } 
    }

    // TODO: is it within 10%?
    // error,  close everything down
    if( hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 300 )
    {
        return 4;           
    }   

    //todo: consistent interface to each label of toolbar and LCD, including vref/vout
    system_config.psu=1;
    system_config.psu_error=false;
    system_config.psu_current_error=false;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
    monitor_clear_current(); //reset current so the LCD gets all characters

    if(fuse_en)
    {
        system_config.psu_irq_en=true;    
    }

    return 0;
}

void psu_enable(opt_args (*args), struct command_result *res)
{
    float volts,current;

    system_config.psu=0;
    system_config.pin_labels[0]=0;
    system_config.pin_changed=0xff;
    system_pin_claim(false, BP_VOUT, 0, 0);

    //prompt voltage (float)
    printf("%sPower supply\r\nVolts (0.80V-5.00V)%s", ui_term_color_info(), ui_term_color_reset());  
    prompt_result result;
    ui_prompt_float(&result, 0.8f, 5.0f, 3.3f, true, &volts);
    if(result.exit)
    {
        res->error=true;
        return;
    }

    uint32_t psu_v_per_bit=((PSU_V_RANGE)/PWM_TOP);
    uint32_t vset=(uint32_t)((float)volts * 10000);
    vset=vset-(PSU_V_LOW*10);
    vset=vset/psu_v_per_bit;
    if(vset>PWM_TOP) vset=PWM_TOP+1;

    system_config.psu_voltage=((vset*psu_v_per_bit)+(PSU_V_LOW*10));
    //printf("vset: %d, psu_v_per_bit: %d", vset,psu_v_per_bit);
    // actual voltage
    float vact=(float)((float)((vset*psu_v_per_bit)+(PSU_V_LOW*10))/(float)10000);
    printf("%s%1.2f%sV%s requested, closest value: %s%1.2f%sV\r\n", 
        ui_term_color_num_float(), volts, ui_term_color_reset(), ui_term_color_info(),
        ui_term_color_num_float(), vact, ui_term_color_reset()    
    );

    // inverse for VREG margining
    vset=(PWM_TOP-vset);
    system_config.psu_dac_v_set=vset;

    //prompt current (float) or none
    //override the current set system
    //TODO: make i limit optional 
    uint32_t isense_en=0;
    printf("Set current limit?\r\n");
    do{
        isense_en=ui_prompt_yes_no();
    }while(isense_en>1);

    printf("\r\n");

    float iset=(float)PWM_TOP;
    if(isense_en==1)
    {   
        printf("\r\n%sMaximum current (0mA-500mA)%s", ui_term_color_info(), ui_term_color_reset());
        ui_prompt_float(&result, 0.0f, 500.0f, 100.0f, true, &current);
        if(result.exit)
        {
            res->error=true;
            return;    
        }

        float psu_i_per_bit= ((float)(PSU_I_RANGE)/(float)PWM_TOP);
        iset= (float)((float)current * 10000);
        iset/=psu_i_per_bit;
        system_config.psu_dac_i_set=(uint32_t)iset;
        system_config.psu_current_limit=(uint32_t)((float)iset*(float)psu_i_per_bit);
        float iact=(float)((float)iset*psu_i_per_bit)/10000;
        printf("%s%1.1f%smA%s requested, closest value: %s%3.1f%smA\r\n", 
            ui_term_color_num_float(), current, ui_term_color_reset(), ui_term_color_info(),
            ui_term_color_num_float(), iact, ui_term_color_reset()    
        );
    }

    system_config.psu_error=true;

    //Current adjust is slice 4 channel a
    //voltage adjust is slice 4 channel b
    uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
    uint v_chan_num= pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
    uint i_chan_num= pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);  
    // first we start with full current because the inrush will often trip the fuse on low limits
    pwm_set_chan_level(slice_num, v_chan_num, (uint16_t)vset);
    pwm_set_chan_level(slice_num, i_chan_num, PWM_TOP);    

    psu_fuse_reset();
    psu_vreg_enable(true);
    busy_wait_ms(10);

    if(isense_en==0)
    { 
        shift_set_clear_wait(CURRENT_EN_OVERRIDE,0);
    }
    else
    {
        //after a delay for inrush, we set the actual limit
        pwm_set_chan_level(slice_num, i_chan_num, (uint16_t)iset);  
        busy_wait_ms(1);
        
        amux_sweep();

        // did the fuse blow?
        // error,  close everything down
        if( hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT] < 100 )
        {
            ui_term_error_report(T_PSU_CURRENT_LIMIT_ERROR);
            psu_reset();
            res->error=true;
            return;   
        } 
    }
    
    amux_sweep();
    // TODO: is it within 10%?
    // error,  close everything down
    if( hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 100 )
    {
        ui_term_error_report(T_PSU_SHORT_ERROR);
        psu_reset();
        res->error=true;
        return;           
    }   

    // is vreg_vout < vref_vout?
    // backflow prevention active
    if( hw_adc_raw[HW_ADC_MUX_VREF_VOUT] > (hw_adc_raw[HW_ADC_MUX_VREG_OUT]+100) ) //+100? TODO: fine tuning
    {
        printf("%s\r\nWarning:\r\n\tBackflow prevention circuit is active\r\n\tVout/Vref voltage is higher than the on-board power supply\r\n\tIs an external supply connected to Vout/Vref?\r\n%s", ui_term_color_warning(), ui_term_color_reset());
    }
    
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
    
    // print voltage and current
    uint32_t isense=((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000)/4095));
    printf("%s\r\nVreg output: %s%d.%d%sV%s, Vref/Vout pin: %s%d.%d%sV%s, Current sense: %s%d.%d%smA%s\r\n%s", 
    ui_term_color_notice(), 
    ui_term_color_num_float(), ((hw_adc_voltage[HW_ADC_MUX_VREG_OUT])/1000), (((hw_adc_voltage[HW_ADC_MUX_VREG_OUT])%1000)/100), ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_num_float(), ((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])/1000), (((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])%1000)/100), ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_num_float(), (isense/1000), ((isense%1000)/100),ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_reset()
    );

    //gpio_set_irq_enabled_with_callback(CURRENT_DETECT, 0b0001, true, &psu_irq_callback);
    //since we dont have any more pins, the over current detect system is read through the 
    //4067 and ADC. It will be picked up in the second core loop
    if(isense_en==1)
    {
        system_config.psu_irq_en=true;
    }
    
    return;
}

void psu_disable(opt_args (*args), struct command_result *res)
{
    //we disable it before an error just for good measure
    if( !psu_reset() )
    {
        system_config.psu_error=true;
        res->error=true;
        return;    
    }

    printf("%s%s: %s%s\r\n",
        ui_term_color_notice(),
        t[T_MODE_POWER_SUPPLY],
        ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );    
    
    psu_cleanup();
}

//cleanup on mode exit, etc
void psu_cleanup(void)
{
    system_config.psu_error=false;
    system_config.psu=0;
    system_config.info_bar_changed=true;
    monitor_clear_current(); //reset current so the LCD gets all characters next time
}

bool psu_setup(void)
{
    uint16_t value;
        
    system_config.psu=0;
    system_config.psu_error=true;

    psu_vreg_enable(false);

    //Current adjust is slice 4 channel a
    //voltage adjust is slice 4 channel b
    uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
    uint v_chan_num= pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
    uint i_chan_num= pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);

    //10KHz clock, into our 1K + 0.1uF filter
    pwm_set_clkdiv_int_frac(slice_num, 16>>4,16&0b1111);
    pwm_set_wrap(slice_num,PWM_TOP);

    //start with v adjust high (lowest voltage output)
    pwm_set_chan_level(slice_num, v_chan_num, PWM_TOP);

    //start with i adjust low (lowest current output)
    pwm_set_chan_level(slice_num, i_chan_num, 0);

    //enable output
    gpio_set_function(PSU_PWM_VREG_ADJ, GPIO_FUNC_PWM);
    gpio_set_function(PSU_PWM_CURRENT_ADJ, GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);    
/*
    //small current limit to avoid fuse being half blown out
    if(!psu_dac_write(PSU_DAC_CURRENT_ADJ, 127))
    {
        return false;
    }

    psu_fuse_reset(); //so it isnt floating arouind 1.7v

    if(!gpio_get(CURRENT_DETECT))
    {
        return false;
    }
*/
psu_fuse_reset(); //temp
    system_config.psu_error=false;
    return true;
}

void psu_init(void){
    gpio_set_function(PSU_PWM_CURRENT_ADJ, GPIO_FUNC_SIO);
    gpio_set_dir(PSU_PWM_CURRENT_ADJ, GPIO_OUT);
    gpio_put(PSU_PWM_CURRENT_ADJ, 0);
    gpio_set_function(PSU_PWM_VREG_ADJ, GPIO_FUNC_SIO);
    gpio_set_dir(PSU_PWM_VREG_ADJ, GPIO_OUT);
    gpio_put(PSU_PWM_VREG_ADJ, 1);
    //gpio_set_function(CURRENT_DETECT, GPIO_FUNC_SIO);
    //gpio_set_dir(CURRENT_DETECT, GPIO_IN);
}