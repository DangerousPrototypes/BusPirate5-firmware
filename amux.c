#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"

void amux_init(void)
{
    //mcu_adc_init();
	adc_init(); adc_gpio_init(AMUX_OUT); adc_gpio_init(CURRENT_SENSE);
}

void amux_sweep(void)
{
    
    adc_select_input(AMUX_OUT_ADC);
    for(int i=0; i<HW_ADC_MUX_COUNT; i++)
    {
        shift_adc_select(15); //to clear any charge from a floating pin
        shift_adc_select(i);
        busy_wait_us(1);
        hw_adc_raw[i]=adc_read();
        hw_adc_voltage[i]=hw_adc_to_volts_x2(i); //these are X2 because a resistor divider /2
    }

    adc_select_input(CURRENT_SENSE_ADC);
    shift_adc_select(15); //to clear any charge from a floating pin
    busy_wait_us(1);
    hw_adc_raw[HW_ADC_CURRENT_SENSE]=adc_read();
    hw_adc_voltage[HW_ADC_CURRENT_SENSE]=hw_adc_to_volts_x1(HW_ADC_CURRENT_SENSE); 

}

uint32_t hw_adc_bio(uint8_t bio)
{    
    //mcu_adc_select(AMUX_OUT_ADC);
    adc_select_input(AMUX_OUT_ADC);
    shift_adc_select(15); //to clear any charge from a floating pin
    shift_adc_select(7-bio);
    busy_wait_us(1);
    return adc_read();
    //return (6600*mcu_adc_read())/4096;
}

