#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pirate.h"
#include "command_struct.h"
#include "display/scope.h"

#define XL9555_I2C_ADDR 0x22
#define XL9555_INPORT0 0x00
#define XL9555_INPORT1 0x01
#define XL9555_OUTPORT0 0x02
#define XL9555_OUTPORT1 0x03
#define XL9555_INVERT0 0x04
#define XL9555_INVERT1 0x05
#define XL9555_CONFIG0 0x06
#define XL9555_CONFIG1 0x07

#if 0
//return true for success, false for failure
bool xl9555_write_i2c(uint8_t addr, uint8_t *data, uint8_t len) {  
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
#endif

/// @brief Write and verify a register on an I2C device
/// @param addr 7 bit I2C address
/// @param reg 
/// @param value 
/// @return 
bool xl9555_register_write_verify(uint8_t addr, uint8_t reg, uint8_t *value){
    uint8_t data[3];
    data[0] = reg;
    data[1] = value[0];
    data[2] = value[1];

    i2c_busy_wait(true);
    //write the register
    //if(!xl9555_write_i2c(addr, data, 3)) return false;
    if(i2c_write_blocking(BP_I2C_PORT, addr, data, 3, false) == PICO_ERROR_GENERIC) goto xl9555_register_write_verify_fail;
    //read the register
    //if(!xl9555_write_i2c(addr, data, 1)) return false;
    if(i2c_write_blocking(BP_I2C_PORT, addr, data, 1, false) == PICO_ERROR_GENERIC) goto xl9555_register_write_verify_fail;
    //if(!xl9555_read_i2c(addr, data, 2)) return false;
    if(i2c_read_blocking(BP_I2C_PORT, addr, data, 2, false) == PICO_ERROR_GENERIC) goto xl9555_register_write_verify_fail;
    i2c_busy_wait(false);

    if( (data[0] == value[0]) && (data[1] == value[1]) ){
        return true;
    }else{
        return false;
    }

    xl9555_register_write_verify_fail:
        i2c_busy_wait(false);
        return false;   
}

void xl9555_init(void) {
    //configure i2c and pins
    //configure interrupt pin
    gpio_set_function(BP_I2C_RESET, GPIO_FUNC_SIO);
    gpio_set_dir(BP_I2C_RESET, GPIO_OUT);
    gpio_put(BP_I2C_RESET, 1);

    gpio_set_function(BP_I2C_INTERRUPT, GPIO_FUNC_SIO);
    gpio_set_dir(BP_I2C_INTERRUPT, GPIO_IN);
    gpio_pull_up(BP_I2C_INTERRUPT); 

    i2c_init(BP_I2C_PORT, 400 * 1000);
    gpio_set_function(BP_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BP_I2C_SCL, GPIO_FUNC_I2C);  
}

// 1 = input, 0 = output
void xl9555_output_enable(bool enable) {

}


void xl9555_write_wait(uint8_t *level, uint8_t *direction) {
    
    if(xl9555_register_write_verify(XL9555_I2C_ADDR, XL9555_OUTPORT0, level) == false){
        //printf("I2C write error\r\n");
    }
    if(xl9555_register_write_verify(XL9555_I2C_ADDR, XL9555_CONFIG0, direction) == false){
        //printf("I2C write error\r\n");
    }
}

void xl9555_interrupt_enable(uint16_t mask) {

}

bool xl9555_read_bit(uint16_t bit) {
    //read the register
    uint8_t data[3];

    i2c_busy_wait(true);
    data[0] = XL9555_INPORT0;
    //xl9555_write_i2c(XL9555_I2C_ADDR, data, 1);
    //xl9555_read_i2c(XL9555_I2C_ADDR, data, 2);
    if(i2c_write_blocking(BP_I2C_PORT, XL9555_I2C_ADDR, data, 1, false) == PICO_ERROR_GENERIC) goto xl9555_read_bit_fail;
    if(i2c_read_blocking(BP_I2C_PORT, XL9555_I2C_ADDR, data, 2, false) == PICO_ERROR_GENERIC) goto xl9555_read_bit_fail;
    uint16_t value = (data[1] << 8) | data[0];
    i2c_busy_wait(false);

    return (value & bit) ? 1 : 0;

    xl9555_read_bit_fail:
        i2c_busy_wait(false);
        return false; //TODO: return codes?
}

