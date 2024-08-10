#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"


void lcd_init(void){
    gpio_set_function(DISPLAY_CS, GPIO_FUNC_SIO);
    gpio_put(DISPLAY_CS, 1);
    gpio_set_dir(DISPLAY_CS, GPIO_OUT);

    gpio_set_function(DISPLAY_DP, GPIO_FUNC_SIO);
    gpio_put(DISPLAY_DP, 1);
    gpio_set_dir(DISPLAY_DP, GPIO_OUT);

    #if (BP_VERSION == BP6)
        gpio_set_function(DISPLAY_BACKLIGHT, GPIO_FUNC_SIO);
        gpio_put(DISPLAY_BACKLIGHT, 0);
        gpio_set_dir(DISPLAY_BACKLIGHT, GPIO_OUT);

        gpio_set_function(DISPLAY_RESET, GPIO_FUNC_SIO);
        gpio_put(DISPLAY_RESET, 1);
        gpio_set_dir(DISPLAY_RESET, GPIO_OUT);
    #endif
}

void lcd_backlight_enable(bool enable){
    
    if(enable){
        #if (BP_VERSION == BP5 || BP_VERSION==BP5XL)
            shift_clear_set_wait( 0, (DISPLAY_BACKLIGHT)); 
        #elif (BP_VERSION==BP6)
            gpio_put(DISPLAY_BACKLIGHT,1);
        #else
            #error "Unknown BP_VERSION"
        #endif
    }else{
        #if (BP_VERSION == BP5 || BP_VERSION==BP5XL)
            shift_clear_set_wait( (DISPLAY_BACKLIGHT), 0); 
        #elif (BP_VERSION==BP6)
            gpio_put(DISPLAY_BACKLIGHT,0);
        #else
            #error "Unknown BP_VERSION"
        #endif  
    }
}

// perform a hardware reset of the LCD according to datasheet specs
void lcd_reset(void){
    #if (BP_VERSION == BP5 || BP_VERSION==BP5XL)
        shift_clear_set_wait(DISPLAY_RESET,0);
    #elif (BP_VERSION==BP6)
        gpio_put(DISPLAY_RESET,0);
    #else
        #error "Unknown BP_VERSION"
    #endif
    busy_wait_us(20);
    #if (BP_VERSION == BP5 || BP_VERSION==BP5XL)
        shift_clear_set_wait(0, DISPLAY_RESET);
    #elif (BP_VERSION==BP6)
        gpio_put(DISPLAY_RESET,1);
    #else
        #error "Unknown BP_VERSION"
    #endif
    busy_wait_ms(100);
}