#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "opt_args.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_info.h"
#include "ui/ui_cmdln.h"
#include "freq.pio.h"

#include "commands/global/freq.h"

void freq_single(struct command_result *res){
    uint32_t temp;
	bool has_value = cmdln_args_uint32_by_position(1, &temp);
    if(!has_value) //show config menu
    {
        if(!freq_configure_disable()) res->error=1;
    }
    else //single measurement
    {
        if(!freq_measure(temp, false)) res->error=1;
    }

}

void freq_cont(struct command_result *res){
    uint32_t temp;
	bool has_value=cmdln_args_uint32_by_position(1, &temp);
    if(!has_value) //show config menu
    {
        if(!freq_configure_enable()) res->error=1;
    }
    else //continuous measurement
    {
        if(!freq_measure(temp, true)) res->error=1;
    }
}


bool freq_check_pin_is_available(const struct ui_prompt* menu, uint32_t* i)
{
    //1=channel b can measure frequency
    //label should be 0, not in use
    //PWM on the A channel should not be in use!
    
    //bounds check
    if((*i)>=count_of(bio2bufiopin)) return 0;

    //temp fix for power supply PWM sharing
    #if BP5_REV <= 8
    if((*i)==0 || (*i)==1) return 0;
    #endif
    
    return ( pwm_gpio_to_channel(bio2bufiopin[(*i)])==PWM_CHAN_B && 
            system_config.pin_labels[(*i)+1]==0 && 
            !(system_config.pwm_active & (0b11<<( (uint8_t)((*i)%2?(*i)-1:(*i)) ) ) ) 
            );
}

bool freq_check_pin_is_active(const struct ui_prompt* menu, uint32_t* i)
{
    return (system_config.freq_active & (0x01<<((uint8_t)(*i))));
}

// TODO: use PIO to do measurements/PWM on all A channels, making it fully flexible
uint32_t freq_print(uint8_t pin, bool refresh)
{
    float measured_duty_cycle = 0.f;
    // now do single or continuous measurement on the pin
    float measured_period = freq_measure_period_and_duty_cycle(bio2bufiopin[pin], &measured_duty_cycle);

    float freq_friendly_value;
    uint8_t freq_friendly_units;
    freq_display_hz(&measured_period, &freq_friendly_value, &freq_friendly_units);

    float ns_friendly_value, freq_ns_value;
    uint8_t ns_friendly_units;
    if(measured_period==0.f){
        freq_ns_value=0;
    }else{
        freq_ns_value=((float)1/(float)(measured_period))*(float)1000000000;
    }
    
    freq_display_ns(&freq_ns_value, &ns_friendly_value, &ns_friendly_units);

    printf("%s%s%s IO%s%d%s: %s%.2f%s%s %s%.2f%s%s (%s%.0f%sHz), %s%s:%s %s%.1f%s%%\r%s",
        ui_term_color_info(), t[T_MODE_FREQ_FREQUENCY],  ui_term_color_reset(), 
        ui_term_color_num_float(), pin, ui_term_color_reset(),
        ui_term_color_num_float(),freq_friendly_value,ui_term_color_reset(), ui_const_freq_labels[freq_friendly_units],
        ui_term_color_num_float(),ns_friendly_value, ui_term_color_reset(), ui_const_freq_labels[ns_friendly_units],
        ui_term_color_num_float(),measured_period, ui_term_color_reset(), 
        ui_term_color_info(), t[T_MODE_FREQ_DUTY_CYCLE], ui_term_color_reset(),
        ui_term_color_num_float(), measured_duty_cycle * 100.f, ui_term_color_reset(), (refresh?"":"\n")
    ); 



    return 1;
}

// F.null - setup continuous frequency measurement on IO pin
uint32_t freq_configure_enable(void)
{
    uint32_t pin;

    printf("%s%s%s",ui_term_color_info(), t[T_MODE_FREQ_MEASURE_FREQUENCY], ui_term_color_reset());
    
    static const struct ui_prompt_config freq_menu_enable_config={
        false,false,false,true,
        &ui_prompt_menu_bio_pin,  
        &ui_prompt_prompt_ordered_list, 
        &freq_check_pin_is_available
    };

    static const struct ui_prompt freq_menu_enable={
        T_MODE_CHOOSE_AVAILABLE_PIN,
        0,0,0, 
        0,0,0,
        0, &freq_menu_enable_config
    };    

    prompt_result result;
    ui_prompt_uint32(&result, &freq_menu_enable, &pin);

    if(result.exit)
    {
        return 0;
    }

    if(result.error)
    {	
        //no pins available, show user current pinout
        //TODO: instead show channel/slice representation and explain assigned function
        ui_info_print_pin_names();
        ui_info_print_pin_labels();
        return 0;
    } 
    
    //register the freq active, apply the pin label
    system_bio_claim(true, (uint8_t)pin, BP_PIN_FREQ, ui_const_pin_states[4]);
    system_set_active(true, (uint8_t)pin, &system_config.freq_active);
 
    printf("\r\n%s%s:%s %s on IO%s%d%s\r\n", 
        ui_term_color_notice(),t[T_MODE_FREQ_MEASURE_FREQUENCY],ui_term_color_reset(),
        t[T_MODE_ENABLED], ui_term_color_num_float(),(uint8_t)pin,ui_term_color_reset() 
    );
    
    //initial measurement
    freq_print((uint8_t)pin, false);
    return 1;
}

// f.null - disable an active frequency measurement, if there is only one active disable it, else prompt for the IO to disable 
uint32_t freq_configure_disable(void)
{
    uint32_t pin;
    if(system_config.freq_active == 0) //no freq active, just exit
    {
        printf("Error: Frequency measurement not active on any IO pins");
        return 0;
    } //if only one freq is active, just disable that one
    else if(system_config.freq_active && !(system_config.freq_active & (system_config.freq_active-1)))
    {
        //set freq to disable
        pin=log2(system_config.freq_active);
    }
    else //multiple freqs active, show menu for user to choose which to disable
    {
        printf("%s%s%s",ui_term_color_info(), "Disable frequency measurement", ui_term_color_reset());  
        
        static const struct ui_prompt_config freq_menu_disable_config={
            false,false,false,true,
            &ui_prompt_menu_bio_pin,  
            &ui_prompt_prompt_ordered_list, 
            &freq_check_pin_is_active
        };

        static const struct ui_prompt freq_menu_disable={
            T_MODE_CHOOSE_AVAILABLE_PIN,
            0,0,0, 
            0,0,0,
            0, &freq_menu_disable_config
        };

        prompt_result result;
        ui_prompt_uint32(&result, &freq_menu_disable, &pin);        
        
        if(result.exit)
        {
            return 0;
        }                         
    }

    //disable    
    pwm_set_enabled(pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)pin]), false);
    gpio_set_function(bio2bufiopin[(uint8_t)pin], GPIO_FUNC_SIO);
    bio_input((uint8_t)pin);

    //unregister, remove pin label
    system_bio_claim(false, (uint8_t)pin, 0, 0);
    system_set_active(false, (uint8_t)pin, &system_config.freq_active);

    printf("\r\n%s%s:%s %s on IO%s%d%s", 
        ui_term_color_notice(),t[T_MODE_FREQ_MEASURE_FREQUENCY],ui_term_color_reset(),
        t[T_MODE_DISABLED], ui_term_color_num_float(),(uint8_t)pin,ui_term_color_reset() 
    );
        
    return 1;           

}

// f.x or F.x
uint32_t freq_measure(int32_t pin, int refresh)
{

    //first make sure the freq is present and available
    if(pin>=count_of(bio2bufiopin))
    {
        printf("Pin IO%d is invalid!", pin);
        return 0;
    }
    //even pin with no freq
    if(pwm_gpio_to_channel(bio2bufiopin[pin])!=PWM_CHAN_B)
    {
        printf("IO%d has no frequency measurement hardware!\r\nFreq. measure is currently only possible on odd pins (1,3,5,7).\r\nIn the future we will fix this using the RP2040 PIO.\r\n", pin);
        return 0;
    } 
    //this pin or adjacent pin is PWM
    if(system_config.pwm_active & (0b11<<( (uint8_t)(pin%2?pin-1:pin))))
    {
        printf("IO%d is unavailable beacuse IO%d or IO%d is in use as a frequency generator (PWM)!\r\nIn the future we will fix this using the RP2040 PIO\r\n", pin, pin-1, pin);
        return 0;
    }
    // pin is in use for purposes other than freq
    if(system_config.pin_labels[pin+1]!=0 && !(system_config.freq_active & (0x01<<((uint8_t)pin))))
    {
        printf("IO%d is in use by %s!\r\n", pin, system_config.pin_labels[pin+1]);
        return 0;
    }

    //now do single or continuous measurement on the pin
    if(refresh)
    {
        // press any key to continue
        prompt_result result;
        ui_prompt_any_key_continue(&result, 250, &freq_print, pin, true);
    }

    // print once (also handles final \n for continuous mode)
    freq_print(pin, false);

    return 1;

}

// we pre-calculate the disable bitmask for the enable function
// to avoid blowing out active PWM when we're done with a FREQ measure
static uint8_t freq_irq_disable_bitmask;
//frequency measurement 
int64_t freq_timer_callback(alarm_id_t id, void *user_data) 
{
    //disable all at once (but leave any other PWM slices running)
    pwm_set_mask_enabled(freq_irq_disable_bitmask);
    for(uint8_t i=0; i<count_of(bio2bufiopin); i++)
    {
        if(system_config.freq_active&(0x01<<i))
        {
            uint slice_num=pwm_gpio_to_slice_num(bio2bufiopin[i]);
            gpio_set_function(bio2bufiopin[i], GPIO_FUNC_SIO);
            uint16_t counter=(uint16_t)pwm_get_counter(slice_num);
            float freq=counter*100.f; 
            system_config.freq_config[i].period=freq;
        }
    }    
    return 0;
}


//trigger all frequency measurements at once using only one delay
void freq_measure_period_irq(void)
{
    uint32_t mask=0;

    //dont do anything if user has no active freq measurements
    if(system_config.freq_active==0)
    {
        return;
    }

    for(uint8_t i=0; i<count_of(bio2bufiopin); i++)
    {
        if(system_config.freq_active&(0x01<<i))
        {
            uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[i]);

            pwm_config cfg = pwm_get_default_config();
            pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
            pwm_config_set_clkdiv(&cfg, 1);
            pwm_init(slice_num, &cfg, false);
            gpio_set_function(bio2bufiopin[i], GPIO_FUNC_PWM);
            pwm_set_counter(slice_num, 0);

            //build mask, slices 0-7...
            mask|=(0x01<<slice_num);

        }
        
        // we pre-calculate the disable bitmask for the enable function
        // to avoid blowing out active PWM when we're done with a FREQ measure
        if(system_config.pwm_active&(0x01<<i))
        {
            uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[i]);
            //build mask, slices 0-7...
            mask|=(0x01<<slice_num);
            freq_irq_disable_bitmask|=(0x01<<slice_num);
        }        
    }

    pwm_set_mask_enabled(mask);
    add_alarm_in_ms(10, freq_timer_callback, NULL, false);
}

const uint max_gate_time_ms = 100;
static float freq_measure_reciprocal_and_duty_cycle(uint gpio, float* duty)
{
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    float freq = 0.f;
    uint pio_loaded_offset = pio_add_program(pio, &freq_program);
    freq_program_init(pio, 0, pio_loaded_offset, gpio);
    freq_start_counting(pio, sm);
    uint timeout_ms = 1000;
    while (pio_sm_is_rx_fifo_empty(pio, sm))
    {
        busy_wait_ms(1);
        if(--timeout_ms == 0)
            goto cleanup;
    }

    uint32_t counter_high = ~pio_sm_get_blocking(pio, sm);
    uint32_t counter_low = ~pio_sm_get_blocking(pio, sm);
    freq = clock_get_hz(clk_sys) / (2.0f * (counter_high + counter_low));
    *duty = (float)(counter_high) /(counter_high + counter_low);
cleanup:
    pio_remove_program(pio, &freq_program, pio_loaded_offset);
    pio_sm_unclaim(pio, sm);
    return freq;
}

static float freq_measure_duty_cycle(uint gpio)
{
    // Only the PWM B pins can be used as inputs.
    assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    float duty = 0;
     uint pio_loaded_offset = pio_add_program(pio, &duty_cycle_program);
    duty_cycle_program_init(pio, 0, pio_loaded_offset, gpio);

    bool hyst_turned_on = false;
    if (!gpio_is_input_hysteresis_enabled(gpio))
    {
        gpio_set_input_hysteresis_enabled(gpio, true);
        hyst_turned_on = true;
    }

    uint32_t status = save_and_disable_interrupts();
    duty_cycle_start_counting(pio, sm);
    uint32_t start = timer_hw->timerawl;
    restore_interrupts(status);
    busy_wait_ms(100);
    status = save_and_disable_interrupts();
    duty_cycle_stop_counting(pio, sm);
    uint32_t stop = timer_hw->timerawl;
    restore_interrupts(status);
    uint32_t time_gate_us = stop - start;

    uint32_t counter = ~duty_cycle_get_counter(pio, sm);
    pio_remove_program(pio, &duty_cycle_program, pio_loaded_offset);
    pio_sm_unclaim(pio, sm);

    if (hyst_turned_on)
        gpio_set_input_hysteresis_enabled(gpio, false);

    float counting_rate = clock_get_hz(clk_sys) / 2;
    float max_possible_count = .000001f * counting_rate * time_gate_us;
    gpio_set_function(gpio, GPIO_FUNC_SIO);

    return counter / max_possible_count;
}

float freq_measure_period_and_duty_cycle(uint gpio, float* duty)
{
    // Only the PWM B pins can be used as inputs.
    assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    float freq = 0.f;
    *duty = 0;
    uint pio_loaded_offset = pio_add_program(pio, &freq_pulse_program);
    freq_pulse_program_init(pio, 0, pio_loaded_offset, gpio);

    bool hyst_turned_on = false;
    if (!gpio_is_input_hysteresis_enabled(gpio))
    {
        gpio_set_input_hysteresis_enabled(gpio, true);
        hyst_turned_on = true;
    }
    //Try pulse counting with 100ms gate time 
    uint32_t status = save_and_disable_interrupts();
    freq_pulse_start_counting(pio, sm);
    uint32_t start = timer_hw->timerawl;
    restore_interrupts(status);
    busy_wait_ms(100);
    status = save_and_disable_interrupts();
    freq_pulse_stop_counting(pio, sm);
    uint32_t stop = timer_hw->timerawl;
    restore_interrupts(status);
    uint32_t time_gate_us = stop - start - 4; //experimental correction 2 for release, 5 for debug 

    *duty = -1.f;
    uint32_t counter = ~freq_pulse_get_counter(pio, sm);
    pio_remove_program(pio, &freq_pulse_program, pio_loaded_offset);
    pio_sm_unclaim(pio, sm);

    freq = (1000000.0f * counter) / time_gate_us;
    if (freq <= 20000.f) { //freq too low to be precise by counting cycles.
        freq = freq_measure_reciprocal_and_duty_cycle(gpio, duty);
    }

    if (hyst_turned_on)
        gpio_set_input_hysteresis_enabled(gpio, false);

    if (*duty < 0.f)
        *duty = freq_measure_duty_cycle(gpio);
    return freq;
}

void freq_display_hz(float* freq_hz_value, float* freq_friendly_value, uint8_t* freq_friendly_units)
{
        //friendly frequency value
        uint32_t freq_friendly_divider;
        if((uint32_t)(*freq_hz_value/1000000)>0){
            freq_friendly_divider=1000000;
            *freq_friendly_units=freq_mhz;
        }else if((uint32_t)(*freq_hz_value/1000)>0){
            freq_friendly_divider=1000;
            *freq_friendly_units=freq_khz;
        }else{
            freq_friendly_divider=1;
            *freq_friendly_units=freq_hz;
        }

        *freq_friendly_value=(float)((float)*freq_hz_value/(float)freq_friendly_divider);

}

void freq_display_ns(float* freq_ns_value, float* period_friendly_value, uint8_t* period_friendly_units)
{
        //friendly period value
        uint32_t period_friendly_divider;
        if((uint32_t)(*freq_ns_value/1000000)>0){
            period_friendly_divider=1000000;
            *period_friendly_units=freq_ms;
        }else if((uint32_t)(*freq_ns_value/1000)>0){
            period_friendly_divider=1000;
            *period_friendly_units=freq_us;
        }else{
            period_friendly_divider=1;
            *period_friendly_units=freq_ns;
        }
        *period_friendly_value=(*freq_ns_value/(float)period_friendly_divider);
}
