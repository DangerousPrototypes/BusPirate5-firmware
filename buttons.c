#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/spi.h"
#include "font/font.h"
#include "ui/ui_lcd.h"

//TODO: Menu timeout

static bool menu_timeout;
static bool menu_active=false;
static uint8_t menu_selected=0;

void fill_box(uint16_t y1, uint16_t y2, const uint8_t *color){
        uint16_t x,y;

        lcd_set_bounding_box(0, 240, y1, y2);
        gpio_put(DISPLAY_DP, 1);
        spi_busy_wait(true);
        gpio_put(DISPLAY_CS, 0); 
        for(x=0;x<240;x++){
            for(y=y1;y<=y2;y++){
                spi_write_blocking(BP_SPI_PORT, color, 2);
            }
        }
        gpio_put(DISPLAY_CS, 1);
        spi_busy_wait(false);
}

void buttons_irq_callback(uint gpio, uint32_t events)
{
    if(!menu_active)
    {
        fill_box(0,320, colors_pallet[LCD_BLACK]);
        //fill_box(0,32, colors_pallet[LCD_RED]);
        menu_update(menu_selected, menu_selected);
        menu_active=true;
        menu_selected=0;   
    }
    else
    {

        /*if(gpio==EXT0)
        {
            if(menu_selected<9)
            {
                menu_update(menu_selected, menu_selected+1);
                menu_selected++;
            }
                
        }
        else if (gpio==EXT1)
        {
            if(menu_selected>0)
            {
                menu_update(menu_selected, menu_selected-1); 
                menu_selected--;
            }
        }*/

    }

    gpio_acknowledge_irq(gpio, events);   
    gpio_set_irq_enabled(gpio, events, true);
    //gpio_set_irq_enabled_with_callback(CURRENT_DETECT, 0b0001, true, &buttons_irq_callback);
}

void buttons_init(void){

    //pull both low
    /*gpio_pull_down(EXT0);
    gpio_pull_down(EXT1);
    
    gpio_set_function(EXT0, GPIO_FUNC_SIO);
    gpio_set_dir(EXT0, GPIO_IN);

    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);

    gpio_set_irq_enabled_with_callback(EXT0, GPIO_IRQ_EDGE_RISE, true, &buttons_irq_callback);
    gpio_set_irq_enabled_with_callback(EXT1, GPIO_IRQ_EDGE_RISE, true, &buttons_irq_callback);
    */
    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);
    gpio_pull_down(EXT1);
}