#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/adc.h"
#include "platform/bpi-rev1.h"
#include "mcu/rp2040.h"
#include "shift.h"
#include "pico/bootrom.h"


//TODO: move all this nonsense to the system config
uint16_t hw_adc_raw[HW_ADC_MUX_MAXADC];
uint32_t hw_adc_voltage[HW_ADC_MUX_MAXADC];
//float hw_adc_voltage[HW_ADC_MUX_MAXADC];
// this array references the pin voltages in the order that
// they appear in terminal and LCD for easy loop writeout
//TODO: make a stuct and create via .h
/*float* hw_pin_voltage_ordered[]={
    &hw_adc_voltage[HW_ADC_MUX_VREF_VOUT],
    &hw_adc_voltage[HW_ADC_MUX_BPIO0],
    &hw_adc_voltage[HW_ADC_MUX_BPIO1],
    &hw_adc_voltage[HW_ADC_MUX_BPIO2],
    &hw_adc_voltage[HW_ADC_MUX_BPIO3],
    &hw_adc_voltage[HW_ADC_MUX_BPIO4],
    &hw_adc_voltage[HW_ADC_MUX_BPIO5],
    &hw_adc_voltage[HW_ADC_MUX_BPIO6],
    &hw_adc_voltage[HW_ADC_MUX_BPIO7]
};*/

uint32_t* hw_pin_voltage_ordered[]={
    &hw_adc_voltage[HW_ADC_MUX_VREF_VOUT],
    &hw_adc_voltage[HW_ADC_MUX_BPIO0],
    &hw_adc_voltage[HW_ADC_MUX_BPIO1],
    &hw_adc_voltage[HW_ADC_MUX_BPIO2],
    &hw_adc_voltage[HW_ADC_MUX_BPIO3],
    &hw_adc_voltage[HW_ADC_MUX_BPIO4],
    &hw_adc_voltage[HW_ADC_MUX_BPIO5],
    &hw_adc_voltage[HW_ADC_MUX_BPIO6],
    &hw_adc_voltage[HW_ADC_MUX_BPIO7]
};

void hw_adc_sweep(void){
    for(int i=0; i<HW_ADC_MUX_MAXADC; i++)
    {
        hw_adc_channel_select(i);
        busy_wait_us(80);
        hw_adc_raw[i]=mcu_adc_read();
    }

    for(int i=0; i<HW_ADC_MUX_MAXADC; i++)
    {        
        //hw_adc_voltage[i]= ((hw_adc_raw[i]*6.6)/4096);
        hw_adc_voltage[i]= (6600*hw_adc_raw[i])/4096;
    }

}

void hw_jump_to_bootloader(struct command_attributes *attributes, struct command_response *response)
{
	/* \param usb_activity_gpio_pin_mask 0 No pins are used as per a cold boot. Otherwise a single bit set indicating which
	*                               GPIO pin should be set to output and raised whenever there is mass storage activity
	*                               from the host.
	* \param disable_interface_mask value to control exposed interfaces
	*  - 0 To enable both interfaces (as per a cold boot)
	*  - 1 To disable the USB Mass Storage Interface
	*  - 2 To disable the USB PICOBOOT Interface
	*/
	reset_usb_boot(0x00,0x00);
}