#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"
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

    void pullup_write_i2c(uint8_t addr, uint8_t *data, uint8_t len) {   
        if(i2c_write_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC){
            printf("I2C write error\r\n");
        }
    }

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
                    uint16_t pin_mask = (1<<((i-((BIO_MAX_PINS/2)*idx)) + ( (3-b) *(BIO_MAX_PINS/2))));                
                    configuration_register[idx] &= ~(pin_mask); //pin to output                    
                    if(pullx_direction & (1<<i)){
                        output_port_register[idx] |= pin_mask; //pin high
                    }
                }
            }

        }

        uint8_t i2c_address[2] = {0x21, 0x20};
        for (uint8_t i =0; i<2; i++){
            uint8_t data[3];
            //change the direction
            data[0] = 0x02;
            data[1] = output_port_register[i]&0xff;
            data[2] = (output_port_register[i]>>8)&0xff;
            pullup_write_i2c(i2c_address[i], data, 3);

            //now make pins outputs
            data[0] = 0x06;
            data[1] = ~(configuration_register[i]&0xff);
            data[2] = ~((configuration_register[i]>>8)&0xff);
            pullup_write_i2c(i2c_address[i], data, 3);
        }

        #if 0
        printf("Configuration:");
        pullx_print_bin(configuration_register[0]);
        printf(" ");
        pullx_print_bin(configuration_register[1]);
        printf("\r\nOutput:");
        pullx_print_bin(output_port_register[0]);
        printf(" ");
        pullx_print_bin(output_port_register[1]);
        printf("\r\n");
        #endif
    }

    void pullx_set_pin(uint8_t pin, uint8_t pull, bool pull_up){
        pullx_value[pin] = pull;
        if(pull_up){
            pullx_direction |= 1<<pin;
        }else{
            pullx_direction &= ~(1<<pin);
        }
    }

    void pullx_set_all_update(uint8_t pull, bool pull_up){
        for(uint8_t i=0; i<BIO_MAX_PINS; i++){
            pullx_set_pin(i, pull, pull_up);
        }
        pullx_update();
    }

    void pullx_get_pin(uint8_t pin, uint8_t *pull, bool *pull_up){
        *pull = pullx_value[pin];
        *pull_up = pullx_direction & (1<<pin);
    }

#endif

void pullup_enable(void) {
    #if BP_HW_PULLX
        //to test: all have 10K pullup 
        //pullx_set_all(0xf000, 0xf000);
        pullx_set_all_update(PULLX_10K, true);
    #elif (BP_VER ==5 && BP_REV <= 8)
        shift_clear_set_wait(0, PULLUP_EN);
    #elif ((BP_VER == 5 && BP_REV > 8)) || (BP_VER == XL5)
        shift_clear_set_wait(PULLUP_EN, 0);
    #elif (BP_VER == 6)
        gpio_put(PULLUP_EN, 0);
    #else
        #error "Platform not speficied in pullup.c"
    #endif
}

void pullup_disable(void) {
    #if BP_HW_PULLX
        // 1M pull-down by default
        // pullx_set_all(0x0f00, 0x0000);
        pullx_set_all_update(PULLX_1M, false);
    #elif (BP_VER ==5 && BP_REV <= 8)
        shift_clear_set_wait(PULLUP_EN, 0);
    #elif ((BP_VER == 5 && BP_REV > 8)) || (BP_VER == XL5)
        shift_clear_set_wait(0, PULLUP_EN);
    #elif (BP_VER == 6)
        gpio_put(PULLUP_EN, 1);
    #else
        #error "Platform not speficied in pullup.c"
    #endif
}

void pullup_init(void) {
    #if BP_HW_PULLX
        pullx_set_all_update(PULLX_1M, false);  
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