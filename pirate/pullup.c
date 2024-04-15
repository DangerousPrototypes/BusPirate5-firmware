#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"

void pullup_enable(void){
    #if BP5_REV <= 8
        shift_clear_set_wait(0,PULLUP_EN);  
    #else
        shift_clear_set_wait(PULLUP_EN,0);
    #endif
}

void pullup_disable(void){
    #if BP5_REV <= 8
        shift_clear_set_wait(PULLUP_EN,0); 
    #else
        shift_clear_set_wait(0,PULLUP_EN);
    #endif
}

void pullup_init(void){
    pullup_disable();
}