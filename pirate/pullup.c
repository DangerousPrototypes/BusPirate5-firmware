#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"

void pullup_enable(void){

    #if (BP_VERSION == 5 || BP_VERSION==XL5)
        #if BP_BOARD_REVISION <= 8
            shift_clear_set_wait(0,PULLUP_EN);  
        #else
            shift_clear_set_wait(PULLUP_EN,0);
        #endif
    #elif (BP_VERSION==6)
        gpio_put(PULLUP_EN,0);        
    #else
        #error "Unknown BP_VERSION"
    #endif
}

void pullup_disable(void){
    #if (BP_VERSION == 5 || BP_VERSION==XL5)
        #if BP_BOARD_REVISION <= 8
            shift_clear_set_wait(PULLUP_EN,0); 
        #else
            shift_clear_set_wait(0,PULLUP_EN);
        #endif
    #elif (BP_VERSION==6)
        gpio_put(PULLUP_EN,1);
    #else
        #error "Unknown BP_VERSION"
    #endif
}

void pullup_init(void){
    #if (BP_VERSION == 5 || BP_VERSION==XL5)
        // nothing to do for 5 / 5XL
    #elif (BP_VERSION ==6)
        gpio_set_function(PULLUP_EN, GPIO_FUNC_SIO);
        gpio_set_dir(PULLUP_EN, GPIO_OUT);
    #else
        #error "Unknown BP_VERSION"
    #endif
    pullup_disable();
}