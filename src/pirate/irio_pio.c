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

static struct _pio_config pio_config_rx_mark;
static struct _pio_config pio_config_rx_space;
static struct _pio_config pio_config_tx;
static struct _pio_config pio_config_tx_carrier;

void _pio_irio_tx_init(uint pin_tx, float desired_period_us, float mod_freq){
    #define INSTRUCTIONS_PER_CYCLE 2
    // Get the system clock frequency in Hz
    uint32_t clock_freq_hz = clock_get_hz(clk_sys);

    // Calculate the divider
    float divider = (float)clock_freq_hz * desired_period_us / (1000000.0f*INSTRUCTIONS_PER_CYCLE);

    //bio_buf_output(BIO2);
    pio_config_tx.pio = PIO_MODE_PIO;
    pio_config_tx.sm = 2;
    pio_config_tx.program = &ir_out_program;
    pio_config_tx.offset = pio_add_program(pio_config_tx.pio, pio_config_tx.program);   
    ir_out_program_init(pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset, pin_tx, divider);

    //configure the PWM for the desired frequency
    //TODO: get the mode frequency from the mode_config
    pio_config_tx_carrier.pio = PIO_MODE_PIO;
    pio_config_tx_carrier.sm = 3;
    pio_config_tx_carrier.program = &ir_out_carrier_program;
    pio_config_tx_carrier.offset = pio_add_program(pio_config_tx_carrier.pio, pio_config_tx_carrier.program);
    ir_out_carrier_program_init(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm, pio_config_tx_carrier.offset, pin_tx, mod_freq);    
}

int pio_irio_tx_init(uint pin_num, uint32_t mod_freq){
    // Desired period in microseconds
    float desired_period_us = 1.0f;
    _pio_irio_tx_init(pin_num, desired_period_us, (float)mod_freq);
}

void pio_irio_tx_deinit(uint pin_num){
    pio_remove_program(pio_config_tx.pio, pio_config_tx.program, pio_config_tx.offset);
    pio_remove_program(pio_config_tx_carrier.pio, pio_config_tx_carrier.program, pio_config_tx_carrier.offset);
}

void _pio_irio_rx_init(uint pin_demod, uint pin_pio2pio, float desired_period_us){
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2c_program, &pio_config.pio, &pio_config.sm,
    // &pio_config.offset, dir_sda, 10, true); hard_assert(success);
    #define INSTRUCTIONS_PER_CYCLE 2
    // Get the system clock frequency in Hz
    uint32_t clock_freq_hz = clock_get_hz(clk_sys);

    // Calculate the divider
    float divider = (float)clock_freq_hz * desired_period_us / (1000000.0f*INSTRUCTIONS_PER_CYCLE);

    pio_config_rx_mark.pio = PIO_MODE_PIO;
    pio_config_rx_mark.sm = 0;
    pio_config_rx_mark.program = &ir_in_low_counter_program;
    pio_config_rx_mark.offset = pio_add_program(pio_config_rx_mark.pio, pio_config_rx_mark.program);
    ir_in_low_counter_program_init(pio_config_rx_mark.pio, pio_config_rx_mark.sm, pio_config_rx_mark.offset, pin_demod, pin_pio2pio, divider);

    pio_config_rx_space.pio = PIO_MODE_PIO;
    pio_config_rx_space.sm = 1;
    pio_config_rx_space.program = &ir_in_high_counter_program;
    pio_config_rx_space.offset = pio_add_program(pio_config_rx_space.pio, pio_config_rx_space.program);
    ir_in_high_counter_program_init(pio_config_rx_space.pio, pio_config_rx_space.sm, pio_config_rx_space.offset, pin_pio2pio, divider);
}

//helper function to load for Bus Pirate INFRARED mode
int pio_irio_rx_init(uint pin_num){
    bio_buf_output(BIO0); //inter statemachine communication pin
    // Desired period in microseconds
    float desired_period_us = 1.0f;
    _pio_irio_rx_init(pin_num, bio2bufiopin[BIO0], desired_period_us);   
}

void pio_irio_rx_deinit(uint pin_num){
    pio_remove_program(pio_config_rx_mark.pio, pio_config_rx_mark.program, pio_config_rx_mark.offset);
    pio_remove_program(pio_config_rx_space.pio, pio_config_rx_space.program, pio_config_rx_space.offset);
}

//wait for end of transmission
bool pio_irio_mode_wait_idle(void){
    //wait for end of transmission
    return pio_sm_wait_idle(pio_config_tx.pio, pio_config_tx.sm, 0xfffff);
}

//drain both RX FIFOs for sync
void pio_irio_mode_drain_fifo(void){
    while(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
        uint16_t temp = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
    }    
    while(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
        uint16_t temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
    }
    return;
}

//push a single 16bit MARK and 16bit SPACE to the FIFO
void pio_irio_mode_tx_write(uint32_t *data){
    //push the data to the FIFO, in pairs to prevent the transmitter from sticking 'on'
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, (*data-1)<<16|(*data-1));
    return;
}

//change the modulation frequency of the TX carrier without resetting the PIO
void pio_irio_tx_mod_freq(float mod_freq){
    float div = clock_get_hz(clk_sys) / (2 * (float)mod_freq); 
    pio_sm_set_clkdiv(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm, div);
    busy_wait_us(1);//takes effect in 3 cycles...
}

//sends raw array of 32bit values. 
//upper 16 bits are the mark, lower 16 bits are the space
void pio_irio_tx_frame_raw(float mod_freq, uint16_t pairs, uint32_t *buffer){
    //configure the PWM for the desired frequency
    pio_irio_tx_mod_freq(mod_freq);

    //push the data to the FIFO, in pairs to prevent the transmitter from sticking 'on'
    for(uint8_t i=0; i<pairs; i++){
        pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, buffer[i]);
    }    
    //wait for end of transmission
    pio_sm_wait_idle(pio_config_tx.pio, pio_config_tx.sm, 0xfffff);
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
bool pio_irio_rx_frame_raw(float *mod_freq, uint16_t *pairs, uint32_t *buffer) {
    static uint8_t state = AIR_IDLE;
    uint16_t temp;

    switch(state){
        case AIR_IDLE:
            //when IDLE, drain any high data in the FIFO
            while(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
                if(temp!=0xffff) temp=(uint16_t)(0xffff-temp);
                *pairs=0;
                buffer[*pairs] = temp<<16; 
                *mod_freq=36000.0f; //todo: actual frequency measurement
                state = AIR_SPACE;
            }
            break;
        case AIR_SPACE:
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                if(temp!=0xffff) temp=(uint16_t)(0xffff-temp); 
                buffer[*pairs] |= temp;
                (*pairs)++;
                state = AIR_MARK;
            }
            break;
        case AIR_MARK:
            //last was space, if another space, end of sequence
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                state = AIR_IDLE;
                return true;
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
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
            while(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                high = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                low = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
                if(low!=0xffff) low=(uint16_t)(0xffff-low); 
                printf("$36:%u,", low);
                state = AIR_SPACE;
            }
            break;
        case AIR_SPACE:
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                high = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                if(high!=0xffff) high=(uint16_t)(0xffff-high); 
                printf("%u,", high);
                state = AIR_MARK;
            }
            break;
        case AIR_MARK:
            //last was space, if another space, end of sequence
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                high = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                printf(";\r\n");
                state = AIR_IDLE;
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                low = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
                if(low!=0xffff) low=(uint16_t)(0xffff-low); 
                printf("%u,", low);
                state = AIR_SPACE;
            }
            break;
    }

    return IR_RX_FRAME_OK;
}
