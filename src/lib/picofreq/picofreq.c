// Accurate frequency measurement using a Pi Pico RP2040
// https://github.com/jbentham/picofreq
// See https://iosoft.blog/picofreq for detailed description
//
// Copyright (c) Jeremy P Bentham 2023
// Apache-2.0 License https://www.apache.org/licenses/LICENSE-2.0
//
// v0.01 JPB 28/7/23 Adapted from QSpeed v0.14
// v0.02 JPB 29/7/23 Removed redundant code
#define VERSION "0.02"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"

// Set zero to use edge-counter, 1 to use edge-timer (reciprocal measurement)
#define USE_EDGE_TIMER      0

// GPIO pin numbers
#define FREQ_IN_PIN         7
#define GATE_TIMER_PIN      0

// Parameters for edge-counter gating
#define TIMER_PRESCALE      1     // 8-bit value
#define TIMER_WRAP          31250  // 17-bit value
#define SAMPLE_FREQ         (125000000 / (TIMER_PRESCALE * TIMER_WRAP))

// Parameters for edge-timer: number of samples, and sample interval
#define NUM_EDGE_TIMES      11
#define EDGE_WAIT_USEC      200001

uint gate_dma_chan, gate_dma_dreq, csr_stopval;
uint counter_slice, gate_slice;
uint counter_lastval, counter_overflow;
uint timer_dma_chan;

uint edge_times[NUM_EDGE_TIMES]; 

void gate_timer_init(int pin);
void freq_counter_init(int pin);
void freq_counter_start(void);
bool freq_counter_value_ready(void);
int freq_counter_value(void);
int edge_counter_frequency(void);
void edge_timer_init(void);
void edge_timer_start(void);
int edge_timer_value(void);
float edge_timer_frequency(void);
bool ustimeout(uint *tickp, uint usec);
void msdelay(int msec);

#if 0
int main() 
{
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    uint ledon=0;
    
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    freq_counter_init(FREQ_IN_PIN);
#if USE_EDGE_TIMER    
    edge_timer_init();
#else
    gate_timer_init(GATE_TIMER_PIN);
#endif    
    printf("PicoFreq v" VERSION "\n");
    while (true) 
    {
        gpio_put(LED_PIN, (ledon = !ledon));
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
        freq_counter_start();
        while (!freq_counter_value_ready())
        {
        }
        printf("Frequency %u Hz\n", edge_counter_frequency());
#endif        
    }
}
#endif
// Initialise gate timer, and DMA to control the counter
void gate_timer_init(int pin)
{
    gate_slice = pwm_gpio_to_slice_num(pin);
    io_rw_32 *counter_slice_csr = &pwm_hw->slice[counter_slice].csr;
    
    pwm_set_clkdiv_int_frac(gate_slice, TIMER_PRESCALE, 0);
    pwm_set_wrap(gate_slice, TIMER_WRAP/2 - 1);
    pwm_set_chan_level(gate_slice, PWM_CHAN_B, TIMER_WRAP/4);
    pwm_set_phase_correct(gate_slice, true);
        
    gate_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(gate_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pwm_get_dreq(gate_slice));
    csr_stopval = *counter_slice_csr;
    dma_channel_configure(gate_dma_chan, &cfg, counter_slice_csr, &csr_stopval, 1, false);
    pwm_set_enabled(gate_slice, true);
}

// Initialise frequency counter
void freq_counter_init(int pin) 
{
    assert(pwm_gpio_to_channel(pin) == PWM_CHAN_B);
    counter_slice = pwm_gpio_to_slice_num(pin);

    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv(&cfg, 1);
    pwm_init(counter_slice, &cfg, false);
}

// Start frequency counter
void freq_counter_start(void)
{
    dma_channel_transfer_from_buffer_now(gate_dma_chan, &csr_stopval, 1);
    pwm_set_counter(counter_slice, 0);
    pwm_set_counter(gate_slice, 0);
    counter_lastval = counter_overflow = 0;
    pwm_set_mask_enabled((1 << counter_slice) | (1 << gate_slice));
}
#include "pirate.h"
#include "pirate/bio.h"
void gpio_fall_irq_handler(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) {
        gpio_set_irq_enabled_with_callback(bio2bufiopin[BIO1], GPIO_IRQ_EDGE_FALL, false, NULL);
        // Start the DMA sequence
        dma_channel_transfer_from_buffer_now(gate_dma_chan, &csr_stopval, 1);
        pwm_set_counter(counter_slice, 0);
        pwm_set_counter(gate_slice, 0);
        counter_lastval = counter_overflow = 0;
        pwm_set_mask_enabled((1 << counter_slice) | (1 << gate_slice));
    }
}

// Check for overflow, and check if capture complete
bool freq_counter_value_ready(void)
{
    uint n = pwm_get_counter(counter_slice);
    
    if (n < counter_lastval)
        counter_overflow++;
    counter_lastval = n;
    return (!dma_channel_is_busy(gate_dma_chan));
}

// Get counter value
int freq_counter_value(void)
{
    while (dma_channel_is_busy(gate_dma_chan)) ;
    pwm_set_enabled(gate_slice, false);    
    return((uint16_t)pwm_get_counter(counter_slice) + counter_overflow*65537);
}

// Get frequency value, return -ve if not ready
int edge_counter_frequency(void)
{
    int val = -1;
    
    if (freq_counter_value_ready())
        val = freq_counter_value() * SAMPLE_FREQ;
    return (val);
}

// Initialise DMA to transfer the edge times
void edge_timer_init(void) 
{
    timer_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(timer_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, pwm_get_dreq(counter_slice));
    dma_channel_configure(timer_dma_chan, &cfg, edge_times, &timer_hw->timerawl, NUM_EDGE_TIMES, false);
    pwm_set_wrap(counter_slice, 0);
}

// Start DMA to record pulse times
void edge_timer_start(void)
{
    dma_channel_transfer_to_buffer_now(timer_dma_chan, edge_times, NUM_EDGE_TIMES);
    pwm_set_enabled(counter_slice, true);
}

// Get average of the edge times
int edge_timer_value(void)
{
    uint i=1, n;
    int total=0;
    dma_channel_abort(timer_dma_chan);
    pwm_set_enabled(counter_slice, false);    
    while (i<NUM_EDGE_TIMES && edge_times[i])
    {
        n = edge_times[i] - edge_times[i-1];
        total += n;
        i++;
    }
    return(i>1 ? total / (i - 1) : 0);
}

// Get frequency value from edge timer
float edge_timer_frequency(void)
{
    int val = edge_timer_value();
    return(val ? 1e6 / val : 0);
}

// Delay given number of milliseconds
void msdelay(int msec)
{
    uint ticks;
    
    if (msec)
    {
        ustimeout(&ticks, 0);
        while (!ustimeout(&ticks, msec*1000)) ;
    }
}

// Return non-zero if timeout
bool ustimeout(uint *tickp, uint usec)
{
    uint t = time_us_32(), dt = t - *tickp;

    if (usec == 0 || dt >= usec)
    {
        *tickp = t;
        return (1);
    }
    return (0);
}

// EOF
