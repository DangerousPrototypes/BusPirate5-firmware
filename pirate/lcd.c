#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"

void lcd_backlight_enable(bool enable){
    if(enable) shift_set_clear_wait( (DISPLAY_BACKLIGHT), 0); 
    else shift_set_clear_wait( 0, (DISPLAY_BACKLIGHT)); 
}

// perform a hardware reset of the LCD according to datasheet specs
void lcd_reset(void){
    shift_set_clear_wait(0, DISPLAY_RESET);
    busy_wait_us(20);
    shift_set_clear_wait(DISPLAY_RESET,0);
    busy_wait_ms(100);
}