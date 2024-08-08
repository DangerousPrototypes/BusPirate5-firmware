#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"

void pullup_enable(void){

    #if (BP_VER == 5 || BP_VER==XL5)
        #if BP_REV <= 8
            shift_clear_set_wait(0,PULLUP_EN);  
        #else
            shift_clear_set_wait(PULLUP_EN,0);
        #endif
    #else
        gpio_put(PULLUP_EN,0);        
    #endif
}

void pullup_disable(void){
    #if (BP_VER == 5 || BP_VER==XL5)
        #if BP_REV <= 8
            shift_clear_set_wait(PULLUP_EN,0); 
        #else
            shift_clear_set_wait(0,PULLUP_EN);
        #endif
    #else
        gpio_put(PULLUP_EN,1);
    #endif
}

void pullup_init(void){
    #if (BP_VER >=6)
        gpio_set_function(PULLUP_EN, GPIO_FUNC_SIO);
        gpio_set_dir(PULLUP_EN, GPIO_OUT);
    #endif
    pullup_disable();
}