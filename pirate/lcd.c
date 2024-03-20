#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"

void lcd_backlight_enable(bool enable){
    if(enable) shift_clear_set_wait( 0, (DISPLAY_BACKLIGHT)); 
    else shift_clear_set_wait( (DISPLAY_BACKLIGHT), 0); 
}

// perform a hardware reset of the LCD according to datasheet specs
void lcd_reset(void){
    shift_clear_set_wait(DISPLAY_RESET,0);
    busy_wait_us(20);
    shift_clear_set_wait(0, DISPLAY_RESET);
    busy_wait_ms(100);
}