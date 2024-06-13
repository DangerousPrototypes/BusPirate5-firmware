#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"

// TODO -- consider marking this `volatile`, as value is modified in ISR.
static bool button_pressed = false; 

// poll the value of button button_id
bool button_get(uint8_t button_id){
    return gpio_get(EXT1);
} 
bool button_check_irq(uint8_t button_id){
    if(button_pressed){
        button_pressed=false;
        return true;
    }
    return false;
}
// example irq callback handler, copy for your own uses
void button_irq_callback(uint gpio, uint32_t events){
    gpio_acknowledge_irq(gpio, events);   
    gpio_set_irq_enabled(gpio, events, true);
    button_pressed=true;
}
// enable the irq for button button_id
void button_irq_enable(uint8_t button_id, void *callback){
    button_pressed=false;
    gpio_set_irq_enabled_with_callback(EXT1, GPIO_IRQ_EDGE_RISE, true, callback);
} 
// disable the irq for button button_id
void button_irq_disable(uint8_t button_id){
    gpio_set_irq_enabled(EXT1, GPIO_IRQ_EDGE_RISE, false);
    button_pressed=false;
}
// initialize all buttons
void button_init(void){
    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);
    gpio_pull_down(EXT1);
}