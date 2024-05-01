#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/adc.h"
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
const uint8_t bio2bufiopin[] =
    {
        BUFIO0,
        BUFIO1,
        BUFIO2,
        BUFIO3,
        BUFIO4,
        BUFIO5,
        BUFIO6,
        BUFIO7};
// here we map the short names to
// the buffer direction pin number
const uint8_t bio2bufdirpin[] =
    {
        BUFDIR0,
        BUFDIR1,
        BUFDIR2,
        BUFDIR3,
        BUFDIR4,
        BUFDIR5,
        BUFDIR6,
        BUFDIR7};