#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pirate.h"
#include "command_struct.h"
#include "display/scope.h"

//return true for success, false for failure
bool xl9555_write_i2c(uint8_t addr, uint8_t *data, uint8_t len) {  
    i2c_busy_wait(true); 
    bool result = i2c_write_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC ? false : true;
    i2c_busy_wait(false);
    return result;
}

// return true for success, false for failure
bool xl9555_read_i2c(uint8_t addr, uint8_t *data, uint8_t len) {
    i2c_busy_wait(true);   
    bool result = i2c_read_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC ? false : true;
    i2c_busy_wait(false);   
    return result;
}    

/// @brief Write and verify a register on an I2C device
/// @param addr 7 bit I2C address
/// @param reg 
/// @param value 
/// @return 
bool xl9555_register_write_verify(uint8_t addr, uint8_t reg, uint16_t value){
    uint8_t data[3];
    data[0] = reg;
    data[1] = value & 0xff;
    data[2] = (value >> 8) & 0xff;

    //write the register
    if(!xl9555_write_i2c(addr, data, 3)) return false;
    //read the register
    if(!xl9555_write_i2c(addr, data, 1)) return false;
    if(!xl9555_read_i2c(addr, data, 2)) return false;
    if( (data[0] != (value&0xff)) || (data[1] != ((value>>8)&0xff)) ){
        return false;
    }
    return true;   
}

void xl9555_init(void) {
    //configure i2c and pins
    //configure interrupt pin
    i2c_busy_wait(true);

    gpio_set_function(BP_I2C_RESET, GPIO_FUNC_SIO);
    gpio_set_dir(BP_I2C_RESET, GPIO_OUT);
    gpio_put(BP_I2C_RESET, 1);

    gpio_set_function(BP_I2C_INTERRUPT, GPIO_FUNC_SIO);
    gpio_set_dir(BP_I2C_INTERRUPT, GPIO_IN);
    gpio_pull_up(BP_I2C_INTERRUPT); 

    i2c_init(BP_I2C_PORT, 400 * 1000);
    gpio_set_function(BP_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BP_I2C_SCL, GPIO_FUNC_I2C);  

    i2c_busy_wait(false);  
}

void xl9555_output_enable(bool enable) {

}

void xl9555_write_wait(uint8_t *level, uint8_t *direction) {
    // I inverted it to clear and then set for easier use with amux
    i2c_busy_wait(true);
    
    if(xl9555_register_write_verify(0x22, 0x02, level) == false){
        //printf("I2C write error\r\n");
    }
    if(xl9555_register_write_verify(0x22, 0x06, direction) == false){
        //printf("I2C write error\r\n");
    }

    i2c_busy_wait(false);

}
