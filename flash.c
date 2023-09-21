#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "hardware/timer.h"

void flash_read_id(void){  
    //read the flash ID
    uint8_t flash_out_buf[1], flash_in_buf[3];
    flash_out_buf[0]=0x9f;
    gpio_put(FLASH_CS, 0);
    spi_write_blocking(BP_SPI_PORT, flash_out_buf, 1);
    spi_read_blocking(BP_SPI_PORT, 0xff, flash_in_buf, 3);
    gpio_put(FLASH_CS, 1);

    if(flash_in_buf[0]!=0xef){
        while(1);
    }
}

void flash_init(void){
    // Output Pins
    gpio_set_function(FLASH_CS, GPIO_FUNC_SIO);
    gpio_put(FLASH_CS, 1);
    gpio_set_dir(FLASH_CS, GPIO_OUT);    
}