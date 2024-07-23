#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"

#define BP_BUTTON_SHORT_PRESS_MS 550

// volatile because these are modified in IRQ handler
static volatile bool button_pressed = false;
static volatile absolute_time_t press_start_time;
static volatile enum button_codes button_code = BP_BUTT_NO_PRESS;

// poll the value of button button_id
bool button_get(uint8_t button_id){
    return gpio_get(EXT1);
} 
// check button press type
enum button_codes button_check_press(uint8_t button_id) {
    if (button_pressed) {
        enum button_codes press_code = button_code;
        button_code = BP_BUTT_NO_PRESS;
        button_pressed = false;
        return press_code;
    }
    return BP_BUTT_NO_PRESS;
}
// example irq callback handler, copy for your own uses
void button_irq_callback(uint gpio, uint32_t events){
    if (events & GPIO_IRQ_EDGE_RISE) {
        press_start_time = get_absolute_time();
    }

    if (events & GPIO_IRQ_EDGE_FALL) {
        absolute_time_t press_end_time = get_absolute_time();
        int64_t duration_ms = absolute_time_diff_us(press_start_time, press_end_time) / 1000;

         if (duration_ms >= BP_BUTTON_SHORT_PRESS_MS) {
            button_code=BP_BUTT_LONG_PRESS;
        } else {
            button_code=BP_BUTT_SHORT_PRESS;
        }

        button_pressed=true;
    }
    
    gpio_acknowledge_irq(gpio, events);   
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}
// enable the irq for button button_id
void button_irq_enable(uint8_t button_id, void *callback){
    button_pressed=false;
    gpio_set_irq_enabled_with_callback(EXT1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, callback);
} 
// disable the irq for button button_id
void button_irq_disable(uint8_t button_id){
    gpio_set_irq_enabled(EXT1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    button_pressed=false;
}
// initialize all buttons
void button_init(void){
    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);
    gpio_pull_down(EXT1);
}
