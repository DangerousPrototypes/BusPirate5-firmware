#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "pio_config.h"
#include "irio.pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "mode/infrared-struct.h"
#include "lib/picofreq/picofreq.h"
#include "pirate/irio_pio.h"

static struct _pio_config pio_config_low;
static struct _pio_config pio_config_high;

void pio_irio_init(uint pin_demod, uint pin_pio2pio, float desired_period_us) {
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2c_program, &pio_config.pio, &pio_config.sm,
    // &pio_config.offset, dir_sda, 10, true); hard_assert(success);
    #define INSTRUCTIONS_PER_CYCLE 2
    // Get the system clock frequency in Hz
    uint32_t clock_freq_hz = clock_get_hz(clk_sys);

    // Calculate the divider
    float divider = (float)clock_freq_hz * desired_period_us / (1000000.0f*INSTRUCTIONS_PER_CYCLE);

    pio_config_low.pio = PIO_MODE_PIO;
    pio_config_low.sm = 0;
    pio_config_low.program = &ir_in_low_counter_program;
    pio_config_low.offset = pio_add_program(pio_config_low.pio, pio_config_low.program);
    ir_in_low_counter_program_init(pio_config_low.pio, pio_config_low.sm, pio_config_low.offset, pin_demod, pin_pio2pio, divider);

    pio_config_high.pio = PIO_MODE_PIO;
    pio_config_high.sm = 1;
    pio_config_high.program = &ir_in_high_counter_program;
    pio_config_high.offset = pio_add_program(pio_config_high.pio, pio_config_high.program);
    ir_in_high_counter_program_init(pio_config_high.pio, pio_config_high.sm, pio_config_high.offset, pin_pio2pio, divider);

    freq_counter_init(bio2bufiopin[BIO1]);
#if USE_EDGE_TIMER    
    edge_timer_init();
#else
    gate_timer_init(bio2bufiopin[BIO2]);
#endif    
   //gpio_set_irq_enabled_with_callback(bio2bufiopin[BIO1], GPIO_IRQ_EDGE_FALL, true, &gpio_fall_irq_handler);


    #ifdef BP_PIO_SHOW_ASSIGNMENT
        printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_high.pio), pio_config_high.sm, pio_config_high.offset);
    #endif
}

void pio_irio_cleanup(void) {
    // pio_remove_program_and_unclaim_sm(&i2c_program, pio_config.pio, pio_config.sm, pio_config.offset);
    pio_remove_program(pio_config_low.pio, pio_config_low.program, pio_config_low.offset);
    pio_remove_program(pio_config_high.pio, pio_config_high.program, pio_config_high.offset);
}



void pio_irio_get(void){
    uint32_t low = 0;
    uint32_t high = 0;


#if USE_EDGE_TIMER      
    memset(edge_times, 0, sizeof(edge_times));
    edge_timer_start();
    uint edge_ticks;
    ustimeout(&edge_ticks, 0);
    while (!ustimeout(&edge_ticks, EDGE_WAIT_USEC))
    {
    }
    printf("Frequency %5.3f Hz\n", edge_timer_frequency());
#else   
    if(!gpio_get(bio2bufiopin[BIO1])){
        freq_counter_start();
        while (!freq_counter_value_ready());
        printf("Frequency %uHz\r\n", edge_counter_frequency());
        //gpio_set_irq_enabled_with_callback(bio2bufiopin[BIO1], GPIO_IRQ_EDGE_FALL, true, &gpio_fall_irq_handler);
    }
#endif   
    return;     


    if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
        low = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
        printf("L:%u--", 0xffff-low);
    }
    if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
        high = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
        printf("H:%u--", 0xffff-high);
    }

}


//helper function to load for Bus Pirate INFRARED mode
int pio_irio_mode_init(uint pin_num){
    bio_buf_output(BIO0); //inter statemachine communication pin
    // Desired period in microseconds
    float desired_period_us = 1.0f;
    pio_irio_init(pin_num, bio2bufiopin[BIO0], desired_period_us);
}

void pio_irio_mode_deinit(uint pin_num){
    pio_irio_cleanup();
}

bool pio_irio_mode_wait_idle(void){
    //return pio_sm_wait_idle(pio_config_control.pio, pio_config_control.sm, 0xfffff);
    return true;
}

void pio_irio_mode_drain_fifo(void){
    return;
}

int pio_irio_mode_tx_init(uint pin_num){
    return 0;
}

void pio_irio_mode_tx_deinit(uint pin_num){
    return;
}

void pio_irio_mode_tx_write(uint32_t *data){
    return;
}

//TODO: output AIR formatted strings
ir_rx_status_t pio_irio_mode_get_frame(uint32_t *rx_frame) {
    uint16_t low, high;
    
    if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
        low = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
        printf("L:%u--", 0xffff-low);
    }
    if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
        high = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
        printf("H:%u--", 0xffff-high);
    }
    return IR_RX_FRAME_OK;
}
