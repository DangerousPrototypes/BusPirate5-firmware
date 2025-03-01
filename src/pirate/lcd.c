#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/shift.h"

void lcd_init(void) {
    gpio_set_function(DISPLAY_CS, GPIO_FUNC_SIO);
    gpio_put(DISPLAY_CS, 1);
    gpio_set_dir(DISPLAY_CS, GPIO_OUT);

    gpio_set_function(DISPLAY_DP, GPIO_FUNC_SIO);
    gpio_put(DISPLAY_DP, 1);
    gpio_set_dir(DISPLAY_DP, GPIO_OUT);

    #if BP_HW_IOEXP_595
        //nothing to do
    #elif BP_HW_IOEXP_NONE
        gpio_set_function(DISPLAY_BACKLIGHT, GPIO_FUNC_SIO);
        gpio_put(DISPLAY_BACKLIGHT, 0);
        gpio_set_dir(DISPLAY_BACKLIGHT, GPIO_OUT);

        gpio_set_function(DISPLAY_RESET, GPIO_FUNC_SIO);
        gpio_put(DISPLAY_RESET, 1);
        gpio_set_dir(DISPLAY_RESET, GPIO_OUT);
    #else
        #error "Unknown platform version in lcd.c"
    #endif
}

void lcd_backlight_enable(bool enable) {
    #if BP_HW_IOEXP_595
        if (enable) {
            shift_clear_set_wait(0, (DISPLAY_BACKLIGHT));
        } else {
            shift_clear_set_wait((DISPLAY_BACKLIGHT), 0);
        }
    #elif BP_HW_IOEXP_NONE
        gpio_put(DISPLAY_BACKLIGHT, enable);
    #else
        #error "Unknown platform version in lcd.c"
    #endif
}

// perform a hardware reset of the LCD according to datasheet specs
void lcd_reset(void) {
    #if BP_HW_IOEXP_595
        shift_clear_set_wait(DISPLAY_RESET, 0);
        busy_wait_us(20);
        shift_clear_set_wait(0, DISPLAY_RESET);
        busy_wait_ms(100);
    #elif BP_HW_IOEXP_NONE
        gpio_put(DISPLAY_RESET, 0);
        busy_wait_us(20);
        gpio_put(DISPLAY_RESET, 1);
        busy_wait_ms(100);
    #else
        #error "Unknown platform version in lcd.c"
    #endif
}