#include <stdio.h>
#include <math.h>
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
static struct _pio_config pio_config_rx_mod_freq;
static struct _pio_config pio_config_tx;
static struct _pio_config pio_config_tx_carrier;
static bool rx_loaded = false; //the three receiver programs are in PIO memory
static bool tx_loaded = false; //the two transmitter programs are in PIO memory
//receiver started or FIFOs drained: a frame in progress is gone, the frame functions start over
static bool rx_frame_buf_restart = false;
static bool rx_frame_printf_restart = false;
//a finished carrier measurement waits for a frame since mod_freq_since
static bool mod_freq_waiting = false;
static absolute_time_t mod_freq_since;

int _irio_pio_tx_init(uint pin_tx, float desired_period_us, float mod_freq){
    #define INSTRUCTIONS_PER_CYCLE 2
    // Get the system clock frequency in Hz
    uint32_t clock_freq_hz = clock_get_hz(clk_sys);

    // Calculate the divider
    float divider = (float)clock_freq_hz * desired_period_us / (1000000.0f*INSTRUCTIONS_PER_CYCLE);

    //load both programs or none, pio_add_program() returns an error code when the PIO is full
    int tx_offset = pio_add_program(PIO_MODE_PIO, &ir_out_program);
    int carrier_offset = pio_add_program(PIO_MODE_PIO, &ir_out_carrier_program);
    if(tx_offset < 0 || carrier_offset < 0){
        if(tx_offset >= 0) pio_remove_program(PIO_MODE_PIO, &ir_out_program, tx_offset);
        if(carrier_offset >= 0) pio_remove_program(PIO_MODE_PIO, &ir_out_carrier_program, carrier_offset);
        return (tx_offset < 0) ? tx_offset : carrier_offset;
    }

    //bio_buf_output(BIO2);
    pio_config_tx.pio = PIO_MODE_PIO;
    pio_config_tx.sm = 2;
    pio_config_tx.program = &ir_out_program;
    pio_config_tx.offset = tx_offset;   
    ir_out_program_init(pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset, pin_tx, divider);

    //configure the PWM for the desired frequency
    //TODO: get the mode frequency from the mode_config
    pio_config_tx_carrier.pio = PIO_MODE_PIO;
    pio_config_tx_carrier.sm = 3;
    pio_config_tx_carrier.program = &ir_out_carrier_program;
    pio_config_tx_carrier.offset = carrier_offset;
    ir_out_carrier_program_init(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm, pio_config_tx_carrier.offset, pin_tx, mod_freq);    
    tx_loaded = true;
    return 1;
}

int irio_pio_tx_init(uint pin_num, uint32_t mod_freq){
    // Desired period in microseconds
    float desired_period_us = 1.0f;
    return _irio_pio_tx_init(pin_num, desired_period_us, (float)mod_freq);
}

void irio_pio_tx_deinit(uint pin_num){
    if(!tx_loaded) return;
    tx_loaded = false;
    //stop the state machines before their programs are freed
    pio_sm_set_enabled(pio_config_tx.pio, pio_config_tx.sm, false);
    pio_sm_set_enabled(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm, false);
    //the carrier can stop with the IR LED pin high, leave it low for the next mode on the pin
    pio_sm_exec(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm, pio_encode_set(pio_pins, 0));
    pio_remove_program(pio_config_tx.pio, pio_config_tx.program, pio_config_tx.offset);
    pio_remove_program(pio_config_tx_carrier.pio, pio_config_tx_carrier.program, pio_config_tx_carrier.offset);
    pio_sm_clear_fifos(pio_config_tx.pio, pio_config_tx.sm);
    pio_sm_clear_fifos(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm);
    pio_sm_restart(pio_config_tx.pio, pio_config_tx.sm);
    pio_sm_restart(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm);
}

int _irio_pio_rx_init(uint pin_demod, uint pin_pio2pio, uint pin_learner, float desired_period_us){
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2c_program, &pio_config.pio, &pio_config.sm,
    // &pio_config.offset, dir_sda, 10, true); hard_assert(success);
    #define INSTRUCTIONS_PER_CYCLE 2
    // Get the system clock frequency in Hz
    uint32_t clock_freq_hz = clock_get_hz(clk_sys);

    // Calculate the divider
    float divider = (float)clock_freq_hz * desired_period_us / (1000000.0f*INSTRUCTIONS_PER_CYCLE);

    //load all three programs or none, pio_add_program() returns an error code when the PIO is full
    int mark_offset = pio_add_program(PIO_MODE_PIO, &ir_in_low_counter_program);
    int space_offset = pio_add_program(PIO_MODE_PIO, &ir_in_high_counter_program);
    int mod_freq_offset = pio_add_program(PIO_LOGIC_ANALYZER_PIO, &measure_mod_freq_program);
    if(mark_offset < 0 || space_offset < 0 || mod_freq_offset < 0){
        if(mark_offset >= 0) pio_remove_program(PIO_MODE_PIO, &ir_in_low_counter_program, mark_offset);
        if(space_offset >= 0) pio_remove_program(PIO_MODE_PIO, &ir_in_high_counter_program, space_offset);
        if(mod_freq_offset >= 0) pio_remove_program(PIO_LOGIC_ANALYZER_PIO, &measure_mod_freq_program, mod_freq_offset);
        return (mark_offset < 0) ? mark_offset : (space_offset < 0) ? space_offset : mod_freq_offset;
    }

    pio_config_rx_mark.pio = PIO_MODE_PIO;
    pio_config_rx_mark.sm = 0;
    pio_config_rx_mark.program = &ir_in_low_counter_program;
    pio_config_rx_mark.offset = mark_offset;
    ir_in_low_counter_program_init(pio_config_rx_mark.pio, pio_config_rx_mark.sm, pio_config_rx_mark.offset, pin_demod, pin_pio2pio, divider);

    pio_config_rx_space.pio = PIO_MODE_PIO;
    pio_config_rx_space.sm = 1;
    pio_config_rx_space.program = &ir_in_high_counter_program;
    pio_config_rx_space.offset = space_offset;
    ir_in_high_counter_program_init(pio_config_rx_space.pio, pio_config_rx_space.sm, pio_config_rx_space.offset, pin_pio2pio, divider);
    //printf("Divider: %f\r\n", divider);
    divider = (float)clock_freq_hz * 0.2f / (1000000.0f*INSTRUCTIONS_PER_CYCLE);
    //printf("Divider: %f\r\n", divider);
    pio_config_rx_mod_freq.pio = PIO_LOGIC_ANALYZER_PIO;
    pio_config_rx_mod_freq.sm = 2;
    pio_config_rx_mod_freq.program = &measure_mod_freq_program;
    pio_config_rx_mod_freq.offset = mod_freq_offset;
    measure_mod_freq_program_init(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm, pio_config_rx_mod_freq.offset, pin_learner, divider);
    irio_pio_rx_reset_mod_freq();
    rx_loaded = true;
    rx_frame_buf_restart = true;
    rx_frame_printf_restart = true;
    return 1;
}

//helper function to load for Bus Pirate INFRARED mode
int irio_pio_rx_init(uint pin_num){
    bio_buf_output(BIO0); //inter statemachine communication pin
    // Desired period in microseconds
    float desired_period_us = 1.0f;
    return _irio_pio_rx_init(pin_num, bio2bufiopin[BIO0], bio2bufiopin[BIO1], desired_period_us);
}

void irio_pio_rx_deinit(uint pin_num){
    if(!rx_loaded) return;
    rx_loaded = false;
    //stop the state machines before their programs are freed
    pio_sm_set_enabled(pio_config_rx_mark.pio, pio_config_rx_mark.sm, false);
    pio_sm_set_enabled(pio_config_rx_space.pio, pio_config_rx_space.sm, false);
    pio_sm_set_enabled(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm, false);
    pio_remove_program(pio_config_rx_mark.pio, pio_config_rx_mark.program, pio_config_rx_mark.offset);
    pio_remove_program(pio_config_rx_space.pio, pio_config_rx_space.program, pio_config_rx_space.offset);
    pio_remove_program(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.program, pio_config_rx_mod_freq.offset);
    pio_sm_clear_fifos(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
    pio_sm_clear_fifos(pio_config_rx_space.pio, pio_config_rx_space.sm);
    pio_sm_restart(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
    pio_sm_restart(pio_config_rx_space.pio, pio_config_rx_space.sm);
}

//wait up to timeout_us for the transmitter to send everything in its FIFO
static bool irio_pio_tx_wait_us(uint32_t timeout_us){
    absolute_time_t end = make_timeout_time_us(timeout_us);
    while(!pio_sm_check_idle(pio_config_tx.pio, pio_config_tx.sm)){
        if(time_reached(end)) return false;
    }
    return true;
}

//wait for end of transmission
bool irio_pio_tx_wait_idle(void){
    if(!tx_loaded) return true;
    //wait for end of transmission, a pair in the FIFO can take up to 131ms
    if(irio_pio_tx_wait_us(2000000)){
        if(rx_loaded){
            pio_sm_clear_fifos(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
            pio_sm_clear_fifos(pio_config_rx_space.pio, pio_config_rx_space.sm);
            rx_frame_buf_restart = true;
            rx_frame_printf_restart = true;
        }
        return true;
    }
    return false;
}

//drain both RX FIFOs for sync
void irio_pio_rxtx_drain_fifo(void){
    if(!rx_loaded) return;
    pio_sm_clear_fifos(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
    pio_sm_clear_fifos(pio_config_rx_space.pio, pio_config_rx_space.sm);
    rx_frame_buf_restart = true;
    rx_frame_printf_restart = true;
    return;
}

//push a single 16bit MARK and 16bit SPACE to the FIFO
void irio_pio_tx_write(uint32_t *data){
    if(!tx_loaded) return;
    uint16_t mark = (*data)>>16;
    uint16_t space = (*data)&0xffff;
    //the PIO adds one to each count, a 0 would become 65535
    if(mark) mark--;
    if(space) space--;
    //push the data to the FIFO, in pairs to prevent the transmitter from sticking 'on'
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, ((uint32_t)mark<<16)|space);
    return;
}

//change the modulation frequency of the TX carrier without resetting the PIO
void irio_pio_tx_set_mod_freq(float mod_freq){
    if(mod_freq==0.0f) mod_freq=38000.0f; //no carrier was measured, use the common 38kHz
    float div = clock_get_hz(clk_sys) / (2 * (float)mod_freq); 
    if(div > 65535.0f) div = 65535.0f; //the slowest the PIO can go, a lower carrier would wrap
    pio_sm_set_clkdiv(pio_config_tx_carrier.pio, pio_config_tx_carrier.sm, div);
    busy_wait_us(1);//takes effect in 3 cycles...
}

//sends raw array of 32bit values. 
//upper 16 bits are the mark, lower 16 bits are the space
void irio_pio_tx_frame_write(float mod_freq, uint16_t pairs, uint32_t *buffer){
    if(!tx_loaded) return;
    //configure the PWM for the desired frequency
    irio_pio_tx_set_mod_freq(mod_freq);

    //push the data to the FIFO, in pairs to prevent the transmitter from sticking 'on'
    uint32_t frame_us = 0;
    for(uint16_t i=0; i<pairs; i++){
        pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, buffer[i]);
        frame_us += (buffer[i]>>16) + (buffer[i]&0xffff) + 4; //each count is 1us
    }    
    //wait for end of transmission, the FIFO can still hold several long pairs
    if(!irio_pio_tx_wait_us(frame_us + 1000)){
        printf("PIO TX timeout\r\n");
    }
    return;
}

void irio_pio_rx_reset_mod_freq(void){
    pio_sm_clear_fifos(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm);
    pio_interrupt_clear(pio_config_rx_mod_freq.pio, 0);
    mod_freq_waiting = false;
}

//the learner sees IR the demodulator doesn't (noise, other carriers): a measurement
//no frame has used for 20ms is not from the next frame, measure again
static void irio_pio_rx_drop_stale_mod_freq(void){
    if(!pio_interrupt_get(pio_config_rx_mod_freq.pio, 0)){
        mod_freq_waiting = false;
        return;
    }
    if(!mod_freq_waiting){
        mod_freq_waiting = true;
        mod_freq_since = get_absolute_time();
    }else if(absolute_time_diff_us(mod_freq_since, get_absolute_time()) > 20000){
        irio_pio_rx_reset_mod_freq();
    }
}

//false = no carrier measurement, mod_freq and us are then 0
bool irio_pio_get_freq_mod(float *mod_freq, uint16_t *us){
    *mod_freq = 0.0f;
    *us = 0;
    if(!pio_interrupt_get(pio_config_rx_mod_freq.pio, 0)) return false;

    typedef enum _transition_state{
        MARK,
        SPACE,
        DONE,
        ERROR
    }transition_state;

    transition_state state=MARK;
    uint32_t mark=0, space=0;
    //instead of getting 8 words directly, we drain the FIFO so it works
    //without change if the PIO is set for 4 or 8 deep FIFO
    while(!pio_sm_is_rx_fifo_empty(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm)){
        uint32_t temp = pio_sm_get(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm);

        //we want to count the 0s and then the first string of 1s
        //this will give us the modulation duration, which we can use to calculate the frequency
        //00000000000000000000000000000000 00011111111111111111111111111111 
        //11111111111111111111111111111111 11111111111111111111111111111111 
        //11111111110000000000000000000000 00000000000011111111111111111111 
        //11111111111111111111111111111111 11111111111111111111111111111111
        for (int j = 31; j >= 0; j--) {
            uint32_t mask = 1 << j;
            switch(state){
                case MARK:
                    if (temp & mask) {
                        if(mark==0){ 
                            state=ERROR;
                        }else{
                            state=SPACE;
                            space++;
                        }
                        break;
                    }
                    mark++;
                    break;
                case SPACE:
                    if (temp & mask) {
                        space++;
                    } else {
                        state=DONE;
                    }
                    break;
                default:
                    break;
            }

        }
    }

    if(state!=DONE){ //there was an error
        return false;
    }
    *us = (mark+space);
    *mod_freq = 1000000.0f/((float)(mark+space)/5.0f);
    return true;        
}

//On timeout, the PIO program increments the counter from 0 to 0xffff
//however there is no such issue during a normal transition
//so only reincrement on timeout
// returns true if a frame is ready, false if no frame is ready
// a signal longer than max_pairs is cut to max_pairs, the rest is dropped until the signal ends
bool irio_pio_rx_frame_buf(float *mod_freq, uint16_t *us, uint16_t *pairs, uint32_t *buffer, uint16_t max_pairs) {
    enum {
        AIR_RESET,
        AIR_IDLE,
        AIR_MARK,
        AIR_SPACE,
        AIR_FULL
    };
    
    static uint8_t state = AIR_RESET;
    uint16_t temp;

    if(!rx_loaded) return false; //no receiver, nothing to read

    if(rx_frame_buf_restart){
        rx_frame_buf_restart = false;
        state = AIR_RESET;
    }

    switch(state){
        case AIR_RESET:
            irio_pio_rx_reset_mod_freq();
            state = AIR_IDLE;
            break;
        case AIR_IDLE:
            //when IDLE, drain any high data in the FIFO
            //while(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
            //    temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
            //}
            pio_sm_clear_fifos(pio_config_rx_space.pio, pio_config_rx_space.sm);
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
                if(temp!=0xffff) temp=(uint16_t)(0xffff-temp);
                *pairs=0;
                buffer[*pairs] = temp<<16; 
                //*mod_freq=36000.0f; //todo: actual frequency measurement
                state = AIR_SPACE;
            }else{
                irio_pio_rx_drop_stale_mod_freq(); //no frame has started yet
            }
            break;
        case AIR_SPACE:
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                if(temp!=0xffff){
                    temp=(uint16_t)(0xffff-temp); 
                    buffer[*pairs] |= temp;
                    (*pairs)++;
                    state = (*pairs < max_pairs) ? AIR_MARK : AIR_FULL;
                }else{
                    buffer[*pairs] |= temp;
                    (*pairs)++;
                    irio_pio_get_freq_mod(mod_freq, us);
                    state = AIR_RESET; //timeout, note final space and go to idle
                    return true;
                } 
            }          
            break;
        case AIR_FULL:
            //buffer full, drop the rest of the signal until the space timeout ends it
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
            }
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                if(temp==0xffff){
                    buffer[(*pairs)-1] |= 0xffff; //end the cut frame with the timeout space, like any other
                    irio_pio_get_freq_mod(mod_freq, us);
                    state = AIR_RESET;
                    return true;
                }
            }
            break;
        case AIR_MARK:
            //last was space, if another space, end of sequence
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                temp = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                irio_pio_get_freq_mod(mod_freq, us);
                state = AIR_RESET;
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
ir_rx_status_t irio_pio_rx_frame_printf(uint32_t *rx_frame) {
    enum {
        AIR_RESET,
        AIR_IDLE,
        AIR_MARK,
        AIR_SPACE
    };
    
    uint16_t low, high;
    static uint8_t state = AIR_RESET;

    if(!rx_loaded) return IR_RX_NO_FRAME; //no receiver, nothing to read

    if(rx_frame_printf_restart){
        rx_frame_printf_restart = false;
        state = AIR_RESET;
    }

    switch(state){
        case AIR_RESET:
            irio_pio_rx_reset_mod_freq();
            state = AIR_IDLE;
            break;
        case AIR_IDLE:
            //when IDLE, drain any high data in the FIFO
            pio_sm_clear_fifos(pio_config_rx_space.pio, pio_config_rx_space.sm);
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_mark.pio, pio_config_rx_mark.sm)){
                low = (uint16_t)pio_sm_get(pio_config_rx_mark.pio, pio_config_rx_mark.sm);
                if(low!=0xffff) low=(uint16_t)(0xffff-low); 
                float mod_freq;
                uint16_t us;
                irio_pio_get_freq_mod(&mod_freq, &us);
                uint8_t mod_freq_int=(uint8_t)roundf(mod_freq/1000.0f);
                printf("$%u:%u,", mod_freq_int, low);
                state = AIR_SPACE;
            }else{
                irio_pio_rx_drop_stale_mod_freq(); //no frame has started yet
            }
            break;
        case AIR_SPACE:
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                high = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                if(high!=0xffff){
                    high=(uint16_t)(0xffff-high); 
                    state = AIR_MARK;
                    printf("%u,", high);
                }else{
                    state = AIR_RESET; //timeout, note final space and go to idle
                    printf("%u,;\r\n", high);
                }
                
            }
            break;
        case AIR_MARK:
            //last was space, if another space, end of sequence
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx_space.pio, pio_config_rx_space.sm)){
                high = (uint16_t)pio_sm_get(pio_config_rx_space.pio, pio_config_rx_space.sm);
                printf(",;\r\n");
                state = AIR_RESET;
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

#if 0
//debug function for developing the RX modulation frequency measurement
void irio_pio_rx_mod_freq_get_debug(float *mod_freq){
    //put 4*32 into the FIFO

    pio_sm_clear_fifos(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm);
    pio_interrupt_clear(pio_config_rx_mod_freq.pio, 0);

    while(true){

        // Poll the IRQ status
        while(!pio_interrupt_get(pio_config_rx_mod_freq.pio, 0));

        typedef enum _transition_state{
            SPACE,
            MARK,
            DONE,
            ERROR
        }transition_state;

        transition_state state=SPACE;
        uint32_t mark=0, space=0;
        //instead of getting 8 words directly, we drain the FIFO so it works
        //without change if the PIO is set for 4 or 8 deep FIFO
        while(!pio_sm_is_rx_fifo_empty(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm)){
            uint32_t temp = pio_sm_get_blocking(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm);

            //we want to count the 0s and then the first string of 1s
            //this will give us the modulation duration, which we can use to calculate the frequency
            //00000000000000000000000000000000 00011111111111111111111111111111 
            //11111111111111111111111111111111 11111111111111111111111111111111 
            //11111111110000000000000000000000 00000000000011111111111111111111 
            //11111111111111111111111111111111 11111111111111111111111111111111
            for (int j = 31; j >= 0; j--) {
                uint32_t mask = 1 << j;
                switch(state){
                    case SPACE:
                        if (temp & mask) {
                            if(space==0){ 
                                state=ERROR;
                            }else{
                                state=MARK;
                                mark++;
                            }
                            break;
                        }
                        space++;
                        break;
                    case MARK:
                        if (temp & mask) {
                            mark++;
                        } else {
                            state=DONE;
                        }
                        break;
                    default:
                        break;
                }

            }


            //write a function to printf the binary representation of a 32bit number
            for (int i = 31; i >= 0; i--) {
                uint32_t mask = 1 << i;
                if (temp & mask) {
                    printf("1");
                } else {
                    printf("0");
                }
            }
            printf(" ");
        }

        float us = (float)(mark+space)/5.0f;
        float hz = 1000000.0f/us;

        if(state!=DONE){
            printf("\r\nERROR: incomplete capture. SPACE: %d, MARK: %d, TOTAL: %d, us: %f\r\n", space, mark, mark+space, us);
        }else{
            printf("\r\nSPACE: %d, MARK: %d, TOTAL: %d, us: %f\r\n", space, mark, mark+space, us);
            printf("Modulation frequency: %fHz, %fkHz\r\n", hz, (float)(hz/1000.0f));

        }

        busy_wait_ms(2000);
                
        // Clear the IRQ
        pio_interrupt_clear(pio_config_rx_mod_freq.pio, 0);
        //printf("%b %b %b %b\r\n", pio_sm_get(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm), pio_sm_get(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm), pio_sm_get(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm), pio_sm_get(pio_config_rx_mod_freq.pio, pio_config_rx_mod_freq.sm));
    }
}
#endif