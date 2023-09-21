#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"
#include "shift.h"

void amux_init(void)
{
    mcu_adc_init();
}

// calculates the voltage
// hi=0 no voltage divider 0 .. 3V3
// hi=1 1/2 voltage divider 0 .. 6V6
float amux_get_voltage(uint8_t channel)
{
	float voltage;

    shift_adc_select(channel);
    uint16_t result = mcu_adc_read();
	voltage=6.6*result;
	voltage/=4096;

	return voltage;
}