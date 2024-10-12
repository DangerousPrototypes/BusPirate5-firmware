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

#if (BP_VER >= 6)
    gpio_set_function(DISPLAY_BACKLIGHT, GPIO_FUNC_SIO);
    gpio_put(DISPLAY_BACKLIGHT, 0);
    gpio_set_dir(DISPLAY_BACKLIGHT, GPIO_OUT);

    gpio_set_function(DISPLAY_RESET, GPIO_FUNC_SIO);
    gpio_put(DISPLAY_RESET, 1);
    gpio_set_dir(DISPLAY_RESET, GPIO_OUT);
#endif
}

void lcd_backlight_enable(bool enable) {

    if (enable) {
#if (BP_VER == 5 || BP_VER == XL5)
        shift_clear_set_wait(0, (DISPLAY_BACKLIGHT));
#else
        gpio_put(DISPLAY_BACKLIGHT, 1);
#endif
    } else {
#if (BP_VER == 5 || BP_VER == XL5)
        shift_clear_set_wait((DISPLAY_BACKLIGHT), 0);
#else
        gpio_put(DISPLAY_BACKLIGHT, 0);
#endif
    }
}

// perform a hardware reset of the LCD according to datasheet specs
void lcd_reset(void) {
#if (BP_VER == 5 || BP_VER == XL5)
    shift_clear_set_wait(DISPLAY_RESET, 0);
#else
    gpio_put(DISPLAY_RESET, 0);
#endif
    busy_wait_us(20);
#if (BP_VER == 5 || BP_VER == XL5)
    shift_clear_set_wait(0, DISPLAY_RESET);
#else
    gpio_put(DISPLAY_RESET, 1);
#endif
    busy_wait_ms(100);
}