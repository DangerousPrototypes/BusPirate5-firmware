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
#include "pirate/bio.h"

static struct _pio_config pio_config_low;
static struct _pio_config pio_config_high;
static struct _pio_config pio_config_tx;

void pio_irio_init(uint pin_demod, uint pin_pio2pio, uint pin_tx, float desired_period_us) {
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

    bio_buf_output(BIO4); //set the buffer to output, maybe this should be done above?
    //bio_buf_output(BIO2);
    pio_config_tx.pio = PIO_MODE_PIO;
    pio_config_tx.sm = 2;
    pio_config_tx.program = &ir_out_program;
    pio_config_tx.offset = pio_add_program(pio_config_tx.pio, pio_config_tx.program);   
    ir_out_program_init(pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset, pin_tx, divider);


    //freq_counter_init(bio2bufiopin[BIO1]);
#if USE_EDGE_TIMER    
    edge_timer_init();
#else
    //gate_timer_init(bio2bufiopin[BIO2]);
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
    pio_remove_program(pio_config_tx.pio, pio_config_tx.program, pio_config_tx.offset);
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
    pio_irio_init(pin_num, bio2bufiopin[BIO0], bio2bufiopin[BIO4], desired_period_us);
}

void pio_irio_mode_deinit(uint pin_num){
    pio_irio_cleanup();
}

bool pio_irio_mode_wait_idle(void){
    //return pio_sm_wait_idle(pio_config_control.pio, pio_config_control.sm, 0xfffff);
    return true;
}

void pio_irio_mode_drain_fifo(void){
    while(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
        uint16_t temp = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
    }    
    while(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
        uint16_t temp = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
    }
    return;
}

int pio_irio_mode_tx_init(uint pin_num){
    return 0;
}

void pio_irio_mode_tx_deinit(uint pin_num){
    return;
}

void pio_irio_mode_tx_write(uint32_t *data){
    
    //configure the PWM for the desired frequency
    uint offset = pio_add_program(PIO_MODE_PIO, &ir_out_carrier_program);   
    ir_out_carrier_program_init(PIO_MODE_PIO, 3, offset, bio2bufiopin[BIO4], 36000.0f);

    //push the data to the FIFO, in pairs to prevent the transmitter from sticking 'on'
    for(uint8_t i=0; i<6; i++){
        pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, (*data-1)<<16|(*data-1));
    }    
    printf("Data: %u\r\n", *data);

    //wait for end of transmission
    pio_sm_wait_idle(pio_config_tx.pio, pio_config_tx.sm, 0xfffff);
    pio_remove_program(PIO_MODE_PIO, &ir_out_carrier_program, offset);
    return;
}

//sends raw array of 32bit values. 
//upper 16 bits are the mark, lower 16 bits are the space
void pio_irio_raw_write_frame(float mod_freq, uint16_t pairs, uint32_t *buffer){
    
    //configure the PWM for the desired frequency
    uint offset = pio_add_program(PIO_MODE_PIO, &ir_out_carrier_program);   
    ir_out_carrier_program_init(PIO_MODE_PIO, 3, offset, bio2bufiopin[BIO4], mod_freq);

    //push the data to the FIFO, in pairs to prevent the transmitter from sticking 'on'
    for(uint8_t i=0; i<pairs; i++){
        pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, buffer[i]);
    }    
    //wait for end of transmission
    pio_sm_wait_idle(pio_config_tx.pio, pio_config_tx.sm, 0xfffff);
    pio_remove_program(PIO_MODE_PIO, &ir_out_carrier_program, offset);
    return;
}

enum {
    AIR_IDLE,
    AIR_MARK,
    AIR_SPACE
};


//On timeout, the PIO program increments the counter from 0 to 0xffff
//however there is no such issue during a normal transition
//so only reincrement on timeout
// returns true if a frame is ready, false if no frame is ready
bool pio_irio_raw_get_frame(float *mod_freq, uint16_t *pairs, uint32_t *buffer) {
    static uint8_t state = AIR_IDLE;
    uint16_t temp;

    switch(state){
        case AIR_IDLE:
            //when IDLE, drain any high data in the FIFO
            while(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
                if(temp!=0xffff) temp=(uint16_t)(0xffff-temp);
                *pairs=0;
                buffer[*pairs] = temp<<16; 
                *mod_freq=36000.0f; //todo: actual frequency measurement
                state = AIR_SPACE;
            }
            break;
        case AIR_SPACE:
            if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
                if(temp!=0xffff) temp=(uint16_t)(0xffff-temp); 
                buffer[*pairs] |= temp;
                (*pairs)++;
                state = AIR_MARK;
            }
            break;
        case AIR_MARK:
            //last was space, if another space, end of sequence
            if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
                state = AIR_IDLE;
                return true;
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
                if(temp!=0xffff) temp=(uint16_t)(0xffff-temp); 
                buffer[*pairs] = temp<<16;
                state = AIR_SPACE;
            }
            break;
    }

    return false;
}



//On timeout, the PIO program increments the counter from 0 to 0xffff
//however there is no such issue during a normal transition
//so only reincrement on timeout
ir_rx_status_t pio_irio_mode_get_frame(uint32_t *rx_frame) {
    uint16_t low, high;
    static uint8_t state = AIR_IDLE;

    switch(state){
        case AIR_IDLE:
            //when IDLE, drain any high data in the FIFO
            while(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
                high = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
                low = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
                if(low!=0xffff) low=(uint16_t)(0xffff-low); 
                printf("$36:%u,", low);
                state = AIR_SPACE;
            }
            break;
        case AIR_SPACE:
            if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
                high = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
                if(high!=0xffff) high=(uint16_t)(0xffff-high); 
                printf("%u,", high);
                state = AIR_MARK;
            }
            break;
        case AIR_MARK:
            //last was space, if another space, end of sequence
            if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
                high = (uint16_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
                printf(";\r\n");
                state = AIR_IDLE;
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
                low = (uint16_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
                if(low!=0xffff) low=(uint16_t)(0xffff-low); 
                printf("%u,", low);
                state = AIR_SPACE;
            }
            break;
    }

    return IR_RX_FRAME_OK;
}
