#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"
#if (BP_VER == 7 && BP_REV == 0)
    #include "hardware/i2c.h"
#endif

#if (BP_VER == 7 && BP_REV == 0)
void pullup_write_i2c(uint8_t addr, uint8_t *data, uint8_t len) {   
    if(i2c_write_blocking(BP_I2C_PORT, addr, data, len, false) == PICO_ERROR_GENERIC){
        printf("I2C write error\n");
    }
}

void pullx_set_all(uint16_t resistor_mask, uint16_t direction_mask){
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

void pullup_enable(void) {
    #if (BP_VER ==5 && BP_REV <= 8)
        shift_clear_set_wait(0, PULLUP_EN);
    #elif ((BP_VER == 5 && BP_REV > 8)) || (BP_VER == XL5)
        shift_clear_set_wait(PULLUP_EN, 0);
    #elif (BP_VER == 6)
        gpio_put(PULLUP_EN, 0);
    #elif (BP_VER == 7)
        //to test: all have 10K pullup 
        pullx_set_all(0xf000, 0xf000);
    #else
        #error "Platform not speficied in pullup.c"
    #endif
}

void pullup_disable(void) {
    #if (BP_VER ==5 && BP_REV <= 8)
        shift_clear_set_wait(PULLUP_EN, 0);
    #elif ((BP_VER == 5 && BP_REV > 8)) || (BP_VER == XL5)
        shift_clear_set_wait(0, PULLUP_EN);
    #elif (BP_VER == 6)
        gpio_put(PULLUP_EN, 1);
    #elif (BP_VER == 7)
        // 1M pull-down by default
        pullx_set_all(0x0f00, 0x0000);
    #else
        #error "Platform not speficied in pullup.c"
    #endif
}

void pullup_init(void) {
    #if (BP_VER == 5 || BP_VER == XL5)
        //nothing to do
    #elif (BP_VER == 6)
        gpio_set_function(PULLUP_EN, GPIO_FUNC_SIO);
        gpio_set_dir(PULLUP_EN, GPIO_OUT);
    #elif (BP_VER == 7)
        //TBD
    #else
        #error "Platform not speficied in pullup.c"
    #endif
    pullup_disable();
}