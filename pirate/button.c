#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"

#define BP_BUTTON_SHORT_PRESS_MS 500 // Threshold for short button press

static bool button_pressed = false;
static bool button_long_pressed = false;
static absolute_time_t press_start_time;

// poll the value of button button_id
bool button_get(uint8_t button_id){
    return gpio_get(EXT1);
} 
bool button_check_irq(uint8_t button_id, bool *long_press){
    if(button_pressed){
        *long_press = button_long_pressed;
        button_pressed=false;
        return true;
    }
    return false;
}
// example irq callback handler, copy for your own uses
void button_irq_callback(uint gpio, uint32_t events){
    // Check if button was pressed (rising edge)
    if (events & GPIO_IRQ_EDGE_RISE) {
        press_start_time =  get_absolute_time();
    }

    // Check if button was released (falling edge)
    if (events & GPIO_IRQ_EDGE_FALL) {
        absolute_time_t press_end_time = get_absolute_time();
        int64_t duration_ms = absolute_time_diff_us(press_start_time, press_end_time) / 1000;
    
    if (duration_ms <= BP_BUTTON_SHORT_PRESS_MS) {
        button_long_pressed = false;
    } else {
        button_long_pressed = true;
    }
    
    button_pressed = true;
}

    gpio_acknowledge_irq(gpio, events);   
    gpio_set_irq_enabled(gpio, events, true);
}

// enable the irq for button button_id
void button_irq_enable(uint8_t button_id, void *callback){
    button_pressed=false;
    gpio_set_irq_enabled_with_callback(EXT1, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true, callback);
} 
// disable the irq for button button_id
void button_irq_disable(uint8_t button_id){
    gpio_set_irq_enabled(EXT1, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, false);
    button_pressed=false;
}
// initialize all buttons
void button_init(void){
    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);
    gpio_pull_down(EXT1);
}
