#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/adc.h"
#include "platform/bpi-rev1.h"
#include "mcu/rp2040.h"
#include "shift.h"
#include "pico/bootrom.h"


//TODO: move all this nonsense to the system config
uint16_t hw_adc_raw[HW_ADC_COUNT];
uint32_t hw_adc_voltage[HW_ADC_COUNT];
// this array references the pin voltages in the order that
// they appear in terminal and LCD for easy loop writeout
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
    
    mcu_adc_select(AMUX_OUT_ADC);
    for(int i=0; i<HW_ADC_MUX_COUNT; i++)
    {
        shift_adc_select(15); //to clear any charge from a floating pin
        hw_adc_channel_select(i);
        busy_wait_us(1);
        hw_adc_raw[i]=mcu_adc_read();
        hw_adc_voltage[i]=hw_adc_to_volts_x2(i); //these are X2 because a resistor divider /2
    }

    mcu_adc_select(CURRENT_SENSE_ADC);
    shift_adc_select(15); //to clear any charge from a floating pin
    busy_wait_us(1);
    hw_adc_raw[HW_ADC_CURRENT_SENSE]=mcu_adc_read();
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

void hw_jump_to_bootloader(struct opt_args *args, struct command_result *res)
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