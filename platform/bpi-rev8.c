#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "platform/bpi-rev8.h"

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

const char hw_pin_label_ordered[][5] = {
    "Vout",
    "IO0",
    "IO1",
    "IO2",
    "IO3",
    "IO4",
    "IO5",
    "IO6",
    "IO7",
    "GND"
};
const uint32_t hw_pin_label_ordered_color[][2] = {
    {BP_COLOR_FULLBLACK, BP_COLOR_RED},
    {BP_COLOR_FULLBLACK, BP_COLOR_ORANGE},
    {BP_COLOR_FULLBLACK, BP_COLOR_YELLOW},
    {BP_COLOR_FULLBLACK, BP_COLOR_GREEN},
    {BP_COLOR_FULLBLACK, BP_COLOR_BLUE},
    {BP_COLOR_FULLBLACK, BP_COLOR_PURPLE},
    {BP_COLOR_FULLBLACK, BP_COLOR_BROWN},
    {BP_COLOR_FULLBLACK, BP_COLOR_GREY},
    {BP_COLOR_FULLBLACK, BP_COLOR_WHITE},
    {BP_COLOR_FULLWHITE, BP_COLOR_FULLBLACK}
};