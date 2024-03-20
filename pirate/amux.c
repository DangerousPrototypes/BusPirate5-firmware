//todo: clean function to change AMUX pins
//clean function to get IO/vreg/vout/current/usb voltage
/*    HW_ADC_MUX_BPIO7, //0
    HW_ADC_MUX_BPIO6, //1
    HW_ADC_MUX_BPIO5, //2
    HW_ADC_MUX_BPIO4, //3
    HW_ADC_MUX_BPIO3, //4
    HW_ADC_MUX_BPIO2, //5
    HW_ADC_MUX_BPIO1, //6
    HW_ADC_MUX_BPIO0, //7
    HW_ADC_MUX_VUSB, //8
    HW_ADC_MUX_CURRENT_DETECT,    //9
    HW_ADC_MUX_VREG_OUT, //10
    HW_ADC_MUX_VREF_VOUT, //11*/
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"
#include "pirate/shift.h"
#include "opt_args.h"
#include "display/scope.h"

static uint8_t amux_current_channel=0xff;

void amux_init(void){
    if (scope_running) // scope is using the analog subsystem
	return;

	adc_init(); 
    adc_gpio_init(AMUX_OUT); 
    adc_gpio_init(CURRENT_SENSE);
}

// select AMUX input source, use the channel defines from the platform header
// only effects the 4067CD analog mux, you cannot get the current measurement from here
bool amux_select_input(uint16_t channel){
    if (scope_running) return false;// scope is using the analog subsystem
    //clear the amux control bits, set the amux channel bits
    shift_clear_set((0b1111<<1), (channel<<1)&0b11110, true);  
    return true;
}

bool amux_select_bio(uint8_t bio){
    if (scope_running) return false;// scope is using the analog subsystem
    amux_select_input(bufio2amux(bio));  
    return true;
}

// read from AMUX using channel list in platform header file
uint32_t amux_read(uint8_t channel){    
    if (scope_running) // scope is using the analog subsystem
	return 0;

    adc_select_input(AMUX_OUT_ADC);
    //if(channel!=amux_current_channel){
        amux_select_input(HW_ADC_MUX_GND); //to clear any charge from a floating pin
        amux_select_input((uint16_t)channel);
        amux_current_channel=channel;
        busy_wait_us(60);
    //}
    return adc_read();
}

// read from AMUX using BIO pin number
uint32_t amux_read_bio(uint8_t bio){    
    if (scope_running) return 0; // scope is using the analog subsystem
    return amux_read(bufio2amux(bio));
}

// this is actually on a different ADC and not the AMUX
// but this is the best place for it I think
// voltage is not /2 so we can use the full range of the ADC
uint32_t amux_read_current(void){
    if (scope_running) return 0;// scope is using the analog subsystem
    adc_select_input(CURRENT_SENSE_ADC);
    return adc_read();
}

// read all the AMUX channels and the current sense
// place into the global arrays hw_adc_raw and hw_adc_voltage
void amux_sweep(void){
    if (scope_running) // scope is using the analog subsystem
        return;
    
    adc_select_input(AMUX_OUT_ADC);
    for(int i=0; i<HW_ADC_MUX_COUNT; i++){
        amux_select_input(HW_ADC_MUX_GND); //to clear any charge from a floating pin
        busy_wait_us(10);
        amux_select_input(i);
        busy_wait_us(60);
        hw_adc_raw[i]=adc_read();
        hw_adc_voltage[i]=hw_adc_to_volts_x2(i); //these are X2 because a resistor divider /2        
    }
    amux_current_channel=HW_ADC_MUX_COUNT-1;
    
    amux_select_input(HW_ADC_MUX_GND); //to clear any charge from a floating pin
    adc_select_input(CURRENT_SENSE_ADC);
    busy_wait_us(60);
    hw_adc_raw[HW_ADC_CURRENT_SENSE]=adc_read();
    hw_adc_voltage[HW_ADC_CURRENT_SENSE]=hw_adc_to_volts_x1(HW_ADC_CURRENT_SENSE); 
}