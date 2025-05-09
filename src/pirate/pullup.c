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
    #define R100K_MASK  0x04

    #define SET_RESISTORS(r2k2, r4k7, r10k, r100k) \
        ((r2k2 ? R2K2_MASK : 0) | (r4k7 ? R4K7_MASK : 0) | (r10k ? R10K_MASK : 0) | (r100k ? R100K_MASK : 0))

    const struct pullx_options_t pullx_options[9] = {
        { .pull=PULLX_OFF, .name="OFF", .resistors=SET_RESISTORS(false, false, false, false) },
        { .pull=PULLX_1K3, .name="1.3K", .resistors=SET_RESISTORS(true, true, true, false) },
        { .pull=PULLX_1K5, .name="1.5K", .resistors=SET_RESISTORS(true, true, false, false) },
        { .pull=PULLX_1K8, .name="1.8K", .resistors=SET_RESISTORS(true, false, true, false) },
        { .pull=PULLX_2K2, .name="2.2K", .resistors=SET_RESISTORS(true, false, false, false) },
        { .pull=PULLX_3K2, .name="3.2K", .resistors=SET_RESISTORS(false, true, true, false) },
        { .pull=PULLX_4K7, .name="4.7K", .resistors=SET_RESISTORS(false, true, false, false) },
        { .pull=PULLX_10K, .name="10K", .resistors=SET_RESISTORS(false, false, true, false) },
        { .pull=PULLX_100K, .name="100K", .resistors=SET_RESISTORS(false, false, false, true) },
    };

    //persistent configuration
    uint8_t pullx_value[BIO_MAX_PINS]={PULLX_OFF,PULLX_OFF,PULLX_OFF,PULLX_OFF,PULLX_OFF,PULLX_OFF,PULLX_OFF,PULLX_OFF};
    uint8_t pullx_direction=0x00; //output direction mask

    bool pullx_register_write_verify(uint8_t addr, uint8_t reg, uint16_t value){
        uint8_t data[3];
        data[0] = reg;
        data[1] = value & 0xff;
        data[2] = (value >> 8) & 0xff;

        i2c_busy_wait(true);
        //write the register
        if(i2c_write_blocking(BP_I2C_PORT, addr, data, 3, false) == PICO_ERROR_GENERIC) goto pullx_register_write_verify_fail;
        //read the register
        if(i2c_write_blocking(BP_I2C_PORT, addr, data, 1, false) == PICO_ERROR_GENERIC) goto pullx_register_write_verify_fail;
        if(i2c_read_blocking(BP_I2C_PORT, addr, data, 2, false) == PICO_ERROR_GENERIC) goto pullx_register_write_verify_fail;
        i2c_busy_wait(false);

        if( (data[0] == (value&0xff)) && (data[1] == ((value>>8)&0xff)) ){
            return true;
        }else{
            return false;
        }

        pullx_register_write_verify_fail:
            i2c_busy_wait(false);
            return false;
    }

    void pullx_print_bin(uint16_t value){
        for(uint8_t i=0; i<16; i++){
            printf("%d", (value & (1<<i)) ? 1 : 0);
        }
    }

    bool pullx_update(void){
        uint16_t output_port_register[2]={0,0};
        uint16_t configuration_register[2]={0xffff,0xffff};
        uint8_t default_pull_down = 0xFF; //load with all pins input

        for(uint8_t i=0; i<BIO_MAX_PINS; i++){
            //build the commands to send to the I2C IO expander
            //if pin is configured as PULLX_OFF, then enable the 1M pull down on the XL9555
            if(pullx_value[i] == PULLX_OFF){
                //set the pin to output/low
                default_pull_down &=~(1<<i); 
                continue;
            }

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
        //OFF 1.3K 1.5K 1.8K 2.2K 3.2K 4.7K 10K 100K
        bool result = true;
        uint8_t i2c_address[2] = {0x21, 0x20};
        for (uint8_t i =0; i<2; i++){
            //enable 1M pull down resistors on the IO expander if the pullx is off
            if(!pullx_register_write_verify(i2c_address[i], 0x02, output_port_register[i]) ||
                !pullx_register_write_verify(i2c_address[i], 0x06, configuration_register[i])
            ){
                default_pull_down = 0x00; //1M all pins on fail
                result = false;
                break;
            }
        }

        //1M default pull down when pullx is off
        ioexp_write_register_dir(0, default_pull_down); //set the pull down resistors

        return result;
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

    static bool pullx_brown_out_reset_enabled = false;

    void pullx_brown_out_reset(uint32_t vout){    
        // TCA6416ARTWT must be reset if voltage drops below 1.65 volts
        if( pullx_brown_out_reset_enabled == false && vout < 1650 ){
            pullx_brown_out_reset_enabled = true;
            //enable the independent 1M pull down resistors on the IO expander
            ioexp_write_register_dir(0, 0x00);
        }else if( pullx_brown_out_reset_enabled == true && vout >= 1650 ){
            pullx_brown_out_reset_enabled = !pullx_update();
        }
    }

#endif

void pullup_enable(void) {
    #if BP_HW_PULLX
        //to test: all have 10K pullup 
        //pullx_set_all(0xf000, 0xf000);
        pullx_brown_out_reset_enabled = false;
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
        pullx_set_all_update(PULLX_OFF, false);
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
        //pullx_set_all_update(PULLX_OFF, false); //defaults to a 1M pull down on the IO expander
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