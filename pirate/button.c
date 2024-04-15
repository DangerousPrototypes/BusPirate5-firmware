#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"

// poll the value of button button_id
bool button_get(uint8_t button_id){
    return gpio_get(EXT1);
} 
// example irq callback handler, copy for your own uses
void button_irq_callback(uint gpio, uint32_t events){
    gpio_acknowledge_irq(gpio, events);   
    gpio_set_irq_enabled(gpio, events, true);
}
// enable the irq for button button_id
void button_irq_enable(uint8_t button_id, void *callback){
    gpio_set_irq_enabled_with_callback(EXT1, GPIO_IRQ_EDGE_RISE, true, callback);
} 
// disable the irq for button button_id
void button_irq_disable(uint8_t button_id){
    gpio_set_irq_enabled(EXT1, GPIO_IRQ_EDGE_RISE, false);
}
// initialize all buttons
void button_init(void){
    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);
    gpio_pull_down(EXT1);
}