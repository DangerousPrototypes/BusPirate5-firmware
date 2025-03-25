#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "pirate/ioexpander.h"

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
// Pin names for terminal and LCD display, in order
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
    "GND"};

// name text and background color for the terminal
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
    {BP_COLOR_FULLWHITE, BP_COLOR_FULLBLACK}};

void hw_pin_defaults(void) {
    gpio_setup(CURRENT_EN_OVERRIDE, GPIO_OUT, 0); 
    gpio_setup(CURRENT_EN, GPIO_OUT, 1);
    gpio_setup(AMUX_S0, GPIO_OUT, 0);
    gpio_setup(AMUX_S1, GPIO_OUT, 1);
    gpio_setup(AMUX_S2, GPIO_OUT, 0);
    gpio_setup(AMUX_S3, GPIO_OUT, 1);
    // FALA pin init
    gpio_setup(LA_BPIO0, GPIO_IN, 0);
    gpio_setup(LA_BPIO1, GPIO_IN, 0);
    gpio_setup(LA_BPIO2, GPIO_IN, 0);
    gpio_setup(LA_BPIO3, GPIO_IN, 0);
    gpio_setup(LA_BPIO4, GPIO_IN, 0);
    gpio_setup(LA_BPIO5, GPIO_IN, 0);
    gpio_setup(LA_BPIO6, GPIO_IN, 0);
    gpio_setup(LA_BPIO7, GPIO_IN, 0);    
}