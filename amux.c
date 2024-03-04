#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"
#include "opt_args.h"
#include "display/scope.h"

void amux_init(void)
{
    if (scope_running) // scope is using the analog subsystem
	return;

    //mcu_adc_init();
	adc_init(); adc_gpio_init(AMUX_OUT); adc_gpio_init(CURRENT_SENSE);
}

void amux_sweep(void)
{
    if (scope_running) // scope is using the analog subsystem
        return;
    
    adc_select_input(AMUX_OUT_ADC);
    for(int i=0; i<HW_ADC_MUX_COUNT; i++)
    {
        shift_adc_select(15); //to clear any charge from a floating pin
        busy_wait_us(10);
        shift_adc_select(i);
        busy_wait_us(60);
        hw_adc_raw[i]=adc_read();
        hw_adc_voltage[i]=hw_adc_to_volts_x2(i); //these are X2 because a resistor divider /2
    }
    
    shift_adc_select(15); //to clear any charge from a floating pin
    adc_select_input(CURRENT_SENSE_ADC);
    busy_wait_us(60);
    hw_adc_raw[HW_ADC_CURRENT_SENSE]=adc_read();
    hw_adc_voltage[HW_ADC_CURRENT_SENSE]=hw_adc_to_volts_x1(HW_ADC_CURRENT_SENSE); 

}

uint32_t hw_adc_bio(uint8_t bio)
{    
    if (scope_running) // scope is using the analog subsystem
	return 0;

    //mcu_adc_select(AMUX_OUT_ADC);
    adc_select_input(AMUX_OUT_ADC);
    shift_adc_select(15); //to clear any charge from a floating pin
    shift_adc_select(7-bio);
    busy_wait_us(1);
    return adc_read();
    //return (6600*mcu_adc_read())/4096;
}

bool amux_check_vout_vref(void)
{
    if (scope_running) // scope is using the analog subsystem
        return false;

    amux_sweep();
    if(hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]<1200)
    {
        printf("Error: No voltage detected on VOUT/VREF pin\r\nHint: Use W to enable the PSU or attach an external supply\r\n");
        return false;
    }    
    return true;
}



