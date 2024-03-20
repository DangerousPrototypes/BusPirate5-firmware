#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "opt_args.h"
#include "display/scope.h"

static uint8_t shift_out[2]={0,0};

void shift_init(void){
    gpio_set_function(SHIFT_EN, GPIO_FUNC_SIO);
    gpio_put(SHIFT_EN, 1); //active low
    gpio_set_dir(SHIFT_EN, GPIO_OUT);
    
    gpio_set_function(SHIFT_LATCH, GPIO_FUNC_SIO);
    gpio_put(SHIFT_LATCH, 0);
    gpio_set_dir(SHIFT_LATCH, GPIO_OUT);
}

void shift_output_enable(bool enable){
    if(enable) gpio_put(SHIFT_EN, 0); //active low, enable shift register outputs
    else gpio_put(SHIFT_EN, 1); //high, disable shift register outputs
    busy_wait_us(5);
}

void shift_clear_set(uint16_t clear_bits, uint16_t set_bits,  bool busy_wait){
    extern uint8_t shift_out[2];
    //I inverted it to clear and then set for easier use with amux
    shift_out[1]&=~((uint8_t)clear_bits);
    shift_out[0]&=~((uint8_t)(clear_bits>>8));
    shift_out[1]|=(uint8_t)set_bits;
    shift_out[0]|=(uint8_t)(set_bits>>8);
    
    if(busy_wait) spi_busy_wait(true);  
    spi_write_blocking(BP_SPI_PORT, shift_out, 2);
    gpio_put(SHIFT_LATCH, 1);
    gpio_put(SHIFT_LATCH, 0);
    if(busy_wait) spi_busy_wait(false);
}

void shift_clear_set_wait(uint16_t clear_bits, uint16_t set_bits)
{
    shift_clear_set(clear_bits, set_bits, true);
}
#if 0
void shift_adc_select(uint8_t channel)
{
    extern uint8_t shift_out[2];

    if (scope_running) // scope is using the analog subsystem
       return;

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
#endif