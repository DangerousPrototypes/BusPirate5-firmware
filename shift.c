#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"

static uint8_t shift_out[2]={0,0};

void shift_init(void)
{
    gpio_set_function(SHIFT_EN, GPIO_FUNC_SIO);
    gpio_put(SHIFT_EN, 1); //active low
    gpio_set_dir(SHIFT_EN, GPIO_OUT);
    
    gpio_set_function(SHIFT_LATCH, GPIO_FUNC_SIO);
    gpio_put(SHIFT_LATCH, 0);
    gpio_set_dir(SHIFT_LATCH, GPIO_OUT);
}

void shift_set_clear(uint16_t set_bits, uint16_t clear_bits, bool busy_wait)
{
    extern uint8_t shift_out[2];
    
    shift_out[1]|=(uint8_t)set_bits;
    shift_out[0]|=(uint8_t)(set_bits>>8);

    shift_out[1]&=~((uint8_t)clear_bits);
    shift_out[0]&=~((uint8_t)(clear_bits>>8));

    if(busy_wait) spi_busy_wait(true);
    
    spi_write_blocking(BP_SPI_PORT, shift_out, 2);
    gpio_put(SHIFT_LATCH, 1);
    //busy_wait_ms(1);
    gpio_put(SHIFT_LATCH, 0);

    if(busy_wait) spi_busy_wait(false);
}

void shift_set_clear_wait(uint16_t set_bits, uint16_t clear_bits)
{
    shift_set_clear(set_bits, clear_bits, true);
}

void shift_adc_select(uint8_t channel)
{
    extern uint8_t shift_out[2];

    {
        extern uint8_t scope_running;
	if (scope_running) {
	    return;
	}
    }
    shift_out[1]&=~((uint8_t)(0b1111<<1)); //clear the amux control bits      
    shift_out[1]|=(uint8_t)(channel<<1); //set the amux channel bits
      
    spi_busy_wait(true);
    
    //uint32_t baud=spi_get_baudrate(BP_SPI_PORT);
    //spi_set_baudrate(BP_SPI_PORT, 1000 * 1000 * 32); // max 10mhz?
    
    spi_write_blocking(BP_SPI_PORT, shift_out, 2);
    gpio_put(SHIFT_LATCH, 1);
    busy_wait_us(5);
    gpio_put(SHIFT_LATCH, 0); 
    
    //spi_set_baudrate(BP_SPI_PORT, baud);   
    spi_busy_wait(false);
}
