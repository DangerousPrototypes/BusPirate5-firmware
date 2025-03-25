#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/ioexpander.h"
#include "pirate/pullup.h"

#if BP_HW_PULLX
    #include "hardware/i2c.h"

    #define R2K2_MASK 0x01
    #define R4K7_MASK 0x02
    #define R10K_MASK 0x08
    #define R1M_MASK  0x04

    #define SET_RESISTORS(r2k2, r4k7, r10k, r1m) \
        ((r2k2 ? R2K2_MASK : 0) | (r4k7 ? R4K7_MASK : 0) | (r10k ? R10K_MASK : 0) | (r1m ? R1M_MASK : 0))

    const struct pullx_options_t pullx_options[9] = {
        { .pull=PULLX_OFF, .name="OFF", .resistors=SET_RESISTORS(false, false, false, false) },
        { .pull=PULLX_1K3, .name="1.3K", .resistors=SET_RESISTORS(true, true, true, false) },
        { .pull=PULLX_1K5, .name="1.5K", .resistors=SET_RESISTORS(true, true, false, false) },
        { .pull=PULLX_1K8, .name="1.8K", .resistors=SET_RESISTORS(true, false, true, false) },
        { .pull=PULLX_2K2, .name="2.2K", .resistors=SET_RESISTORS(true, false, false, false) },
        { .pull=PULLX_3K2, .name="3.2K", .resistors=SET_RESISTORS(false, true, true, false) },
        { .pull=PULLX_4K7, .name="4.7K", .resistors=SET_RESISTORS(false, true, false, false) },
        { .pull=PULLX_10K, .name="10K", .resistors=SET_RESISTORS(false, false, true, false) },
        { .pull=PULLX_1M, .name="1M", .resistors=SET_RESISTORS(false, false, false, true) },
    };

    //persistent configuration
    uint8_t pullx_value[BIO_MAX_PINS]={PULLX_1M,PULLX_1M,PULLX_1M,PULLX_1M,PULLX_1M,PULLX_1M,PULLX_1M,PULLX_1M};
    uint8_t pullx_direction=0x00; //output direction mask

    //return true for success, false for failure
    bool pullup_write_i2c(uint8_t addr, uint8_t *data, uint8_t len) {  
        i2c_busy_wait(true); 
        bool result = i2c_write_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC ? false : true;
        i2c_busy_wait(false);
        return result;
    }
    // return true for success, false for failure
    bool pullup_read_i2c(uint8_t addr, uint8_t *data, uint8_t len) {
        i2c_busy_wait(true);   
        bool result = i2c_read_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC ? false : true;
        i2c_busy_wait(false);   
        return result;
    }    

    bool pullx_register_write_verify(uint8_t addr, uint8_t reg, uint16_t value){
        uint8_t data[3];
        data[0] = reg;
        data[1] = value & 0xff;
        data[2] = (value >> 8) & 0xff;

        //write the register
        if(!pullup_write_i2c(addr, data, 3)) return false;
        //read the register
        if(!pullup_write_i2c(addr, data, 1)) return false;
        if(!pullup_read_i2c(addr, data, 2)) return false;
        if( (data[0] != (value&0xff)) || (data[1] != ((value>>8)&0xff)) ){
            return false;
        }
        return true;   
    }

#if 0
    void pullx_set_all_test(uint16_t resistor_mask, uint16_t direction_mask){
        uint8_t data[3] = {0x06, 0xFF, 0xFF};
        //configuration register, all pins as inputs = 1
        pullup_write_i2c(0x40, data, 3);
        pullup_write_i2c(0x42, data, 3);
        //1M pull down by default
        data[0] = 0x02;
        data[1] = direction_mask&0xff;
        data[2] = (direction_mask>>8)&0xff;
        pullup_write_i2c(0x40, data, 3);
        pullup_write_i2c(0x42, data, 3);
        //now make those pins outputs
        data[0]=0x06;
        data[1]= ~(resistor_mask&0xff);
        data[2]= ~((resistor_mask>>8)&0xff);
        pullup_write_i2c(0x40, data, 3);
        pullup_write_i2c(0x42, data, 3);
    }
#endif
    void pullx_print_bin(uint16_t value){
        for(uint8_t i=0; i<16; i++){
            printf("%d", (value & (1<<i)) ? 1 : 0);
        }
    }

    bool pullx_update(void){
        uint16_t output_port_register[2]={0,0};
        uint16_t configuration_register[2]={0xffff,0xffff};

        for(uint8_t i=0; i<BIO_MAX_PINS; i++){
            //build the commands to send to the I2C IO expander
            for(uint8_t b=0; b<4; b++){
                //if bit is set in the resistor mask, set the resistor pin to output
                //resistor mask is 1 if enabled
                // however the I2C expander has inverted logic, so clear the bit to enable the resistor
                if(pullx_options[pullx_value[i]].resistors & (1<<b)){
                    uint8_t idx = 0;
                    if(i>=4) idx = 1;
                    uint16_t pin_mask = (1<< ((3-(i-(idx*4))) + (b*4)));
                    configuration_register[idx] &= ~(pin_mask); //pin to output                    
                    if(pullx_direction & (1<<i)){
                        output_port_register[idx] |= pin_mask; //pin high
                    }
                }
            }

        }
        //OFF 1.3K 1.5K 1.8K 2.2K 3.2K 4.7K 10K 1M
        uint8_t i2c_address[2] = {0x21, 0x20};
        for (uint8_t i =0; i<2; i++){
            if(!pullx_register_write_verify(i2c_address[i], 0x02, output_port_register[i])) return false;
            if(!pullx_register_write_verify(i2c_address[i], 0x06, configuration_register[i])) return false;
        }
            
#if 0
        printf("Configuration:");
        pullx_print_bin(configuration_register[0]);
        printf(" ");
        pullx_print_bin(configuration_register[1]);

        uint8_t data[3];
        data[0] = 0x06;
        pullup_write_i2c(0x21, data, 1);
        pullup_read_i2c(0x21, data, 2);
        printf("\r\nRead: %02x %02x | ", data[0], data[1]); 
        data[0] = 0x06;
        pullup_write_i2c(0x20, data, 1);
        pullup_read_i2c(0x20, data, 2);
        printf("%02x %02x\r\n", data[0], data[1]);

        printf("\r\nOutput:");
        pullx_print_bin(output_port_register[0]);
        printf(" ");
        pullx_print_bin(output_port_register[1]);

        data[0] = 0x02;
        pullup_write_i2c(0x21, data, 1);
        pullup_read_i2c(0x21, data, 2);
        printf("\r\nRead: %02x %02x | ", data[0], data[1]);
        data[0] = 0x02;
        pullup_write_i2c(0x20, data, 1);
        pullup_read_i2c(0x20, data, 2);
        printf("%02x %02x\r\n", data[0], data[1]);        
        printf("\r\n");
#endif
        return true;
    }

    void pullx_set_pin(uint8_t pin, uint8_t pull, bool pull_up){
        pullx_value[pin] = pull;
        if(pull_up){
            pullx_direction |= 1<<pin;
        }else{
            pullx_direction &= ~(1<<pin);
        }
    }

    bool pullx_set_all_update(uint8_t pull, bool pull_up){
        for(uint8_t i=0; i<BIO_MAX_PINS; i++){
            pullx_set_pin(i, pull, pull_up);
        }
        return pullx_update();
    }

    void pullx_get_pin(uint8_t pin, uint8_t *pull, bool *pull_up){
        *pull = pullx_value[pin];
        *pull_up = pullx_direction & (1<<pin);
    }

    void pullx_brown_out_reset(uint32_t vout){
        static bool pullx_brown_out_reset = false;

        // TCA6416ARTWT must be reset if voltage drops below 1.65 volts
        if( pullx_brown_out_reset == false && vout < 1650 ){
            pullx_brown_out_reset = true;
        }else if( pullx_brown_out_reset == true && vout >= 1650 ){
            pullx_brown_out_reset = !pullx_update();
        }
    }

#endif

void pullup_enable(void) {
    #if BP_HW_PULLX
        //to test: all have 10K pullup 
        //pullx_set_all(0xf000, 0xf000);
        pullx_set_all_update(PULLX_10K, true);
    #elif (BP_VER ==5 && BP_REV <= 8)
        ioexp_clear_set(0, IOEXP_PULLUP_EN);
    #elif ((BP_VER == 5 && BP_REV > 8)) || (BP_VER == XL5)
        ioexp_clear_set(IOEXP_PULLUP_EN, 0);
    #elif (BP_VER == 6)
        gpio_put(PULLUP_EN, 0);
    #else
        #error "Platform not speficied in pullup.c"
    #endif
}

void pullup_disable(void) {
    #if BP_HW_PULLX
        // 1M pull-down by default
        pullx_set_all_update(PULLX_1M, false);
    #elif (BP_VER ==5 && BP_REV <= 8)
        ioexp_clear_set(IOEXP_PULLUP_EN, 0);
    #elif ((BP_VER == 5 && BP_REV > 8)) || (BP_VER == XL5)
        ioexp_clear_set(0, IOEXP_PULLUP_EN);
    #elif (BP_VER == 6)
        gpio_put(PULLUP_EN, 1);
    #else
        #error "Platform not speficied in pullup.c"
    #endif
}

void pullup_init(void) {
    #if BP_HW_PULLX
        //pullx_set_all_update(PULLX_1M, false);  
    #elif (BP_VER == 5 || BP_VER == XL5)
        //nothing to do
    #elif (BP_VER == 6)
        gpio_set_function(PULLUP_EN, GPIO_FUNC_SIO);
        gpio_set_dir(PULLUP_EN, GPIO_OUT);             
    #else
        #error "Platform not speficied in pullup.c"
    #endif
    pullup_disable();
}