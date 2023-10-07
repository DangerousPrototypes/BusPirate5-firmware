#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_info.h"
#include "system_config.h"
#include "freq.h"


uint32_t pwm_get_settings(float* pwm_hz_actual, float* duty_user_value);
uint8_t pwm_freq_find(float* freq_hz_value, float* pwm_hz_actual, float* pwm_ns_actual, uint32_t* pwm_divider, uint32_t* pwm_top);

uint32_t pwm_divider, pwm_top, pwm_duty;

bool pwm_check_pin_is_available(const struct ui_prompt* menu, uint32_t* i)
{
    //label should be 0, not in use
    //FREQ on the B channel should not be in use!
    //PWM should not already be in use on A or B channel of this slice
    
    //bounds check
    if((*i)>=count_of(bio2bufiopin)) return 0;
    
    //temp fix for power supply PWM sharing
    if((*i)==0 || (*i)==1) return 0;
    
    return ( system_config.pin_labels[(*i)+1]==0 && 
            !(system_config.freq_active & (0b11<<( (uint8_t)( (*i)%2 ? (*i)-1 : (*i) ) ) ) ) &&
            !(system_config.pwm_active & (0b11<<( (uint8_t)( (*i)%2 ? (*i)-1 : (*i) ) ) ) )
            );
}

bool pwm_check_pin_is_active(const struct ui_prompt* menu, uint32_t* i)
{
    //bounds check
    if((*i)>=count_of(bio2bufiopin)) return 0;

    return (system_config.pwm_active & ( 0x01 << ((uint8_t)(*i)) ));
}

//TODO: future feature - g.5/G.5 enable/disable PWM with previous setting, prompt if no previous settings
void pwm_configure_enable(opt_args (*args), struct command_result *res)
{
    uint32_t pin;

    printf("%s%s%s",ui_term_color_info(), t[T_MODE_PWM_GENERATE_FREQUENCY], ui_term_color_reset());
    
    static const struct ui_prompt_config freq_menu_enable_config={
        false,false,false,true,
        &ui_prompt_menu_bio_pin,  
        &ui_prompt_prompt_ordered_list, 
        &pwm_check_pin_is_available
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
        (*res).error=true;
        return;
    }

    if(result.error)
    {	
        //no pins available, show user current pinout
        //TODO: instead show channel/slice representation and explain assigned function
        ui_info_print_pin_names();
        ui_info_print_pin_labels();
        (*res).error=true;
        return;
    }

    // populate the variables with settings for PWM 
    if(!pwm_get_settings(&system_config.freq_config[pin].period, &system_config.freq_config[pin].dutycycle))//raw return is used for UI display
    {
        (*res).error=true;
        return;
    }
    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)pin]);
    uint chan_num= pwm_gpio_to_channel((uint8_t)pin);

    pwm_set_clkdiv_int_frac(slice_num, pwm_divider>>4,pwm_divider&0b1111);
    pwm_set_wrap(slice_num, pwm_top);
    pwm_set_chan_level(slice_num, chan_num, pwm_duty);

    bio_buf_output((uint8_t)pin);
    gpio_set_function(bio2bufiopin[(uint8_t)pin], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);
            
    //register the freq active, apply the pin label
    system_bio_claim(true, (uint8_t)pin, BP_PIN_PWM, ui_const_pin_states[3]);
    system_set_active(true, (uint8_t)pin, &system_config.pwm_active);    

    printf("\r\n%s%s:%s %s on IO%s%d%s\r\n", 
        ui_term_color_notice(),t[T_MODE_PWM_GENERATE_FREQUENCY],ui_term_color_reset(),
        t[T_MODE_ENABLED], ui_term_color_num_float(),(uint8_t)pin,ui_term_color_reset() 
    );
 
}

void pwm_configure_disable(opt_args (*args), struct command_result *res)
{
    uint32_t pin;

    if(system_config.pwm_active == 0) //no pwm active, just exit
    {
        printf("Frequency generation not active on any IO pins!");
        (*res).error=true;
        return;
    } 
    else if(!args[0].no_value)
    {
        //first make sure the freq is present and available
        if(args[0].i >= count_of(bio2bufiopin))
        {
            printf("Pin IO%d is invalid!", args[0].i);
            (*res).error=true;
            return;
        }
        pin=args[0].i;

    }//if only one pwm is active, just disable that one
    else if(system_config.pwm_active && !(system_config.pwm_active & (system_config.pwm_active-1)))
    {
        //find pwm to disable
        pin=log2(system_config.pwm_active);
    }
    else //multiple pwm active, show menu for user to choose which to disable
    {
        printf("%s%s%s",ui_term_color_info(), "Disable frequency generation", ui_term_color_reset());      
 
        static const struct ui_prompt_config freq_menu_disable_config={
            false,false,false,true,
            &ui_prompt_menu_bio_pin,  
            &ui_prompt_prompt_ordered_list, 
            &pwm_check_pin_is_active
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
            (*res).error=true;
            return;
        }

        if(result.error)
        {	
            //no pins available, show user current pinout
            //TODO: instead show channel/slice representation and explain assigned function
            ui_info_print_pin_names();
            ui_info_print_pin_labels();
            (*res).error=true;
            return;
        }               
    }

    //no pwm on this pin!
    if( !(system_config.pwm_active&(0x01<<pin)) ) {
        printf("Error: frequency generator not active on IO%d", pin);
        (*res).error=true;
        return;
    }

    //disable    
    pwm_set_enabled(pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)pin]), false);
    gpio_set_function(bio2bufiopin[(uint8_t)pin], GPIO_FUNC_SIO);
    bio_input((uint8_t)pin);
    
    //unregister, remove pin label
    system_bio_claim(false, (uint8_t)pin, 0, 0);
    system_set_active(false, (uint8_t)pin, &system_config.pwm_active);    

    printf("\r\n%s%s:%s %s on IO%s%d%s", 
        ui_term_color_notice(),t[T_MODE_PWM_GENERATE_FREQUENCY],ui_term_color_reset(),
        t[T_MODE_DISABLED], ui_term_color_num_float(),(uint8_t)pin,ui_term_color_reset() 
    );
          
}

//return 0 success, 1 too low, 2 too high
uint8_t pwm_freq_find(float* freq_hz_value, float* pwm_hz_actual, float* pwm_ns_actual, uint32_t* pwm_divider, uint32_t* pwm_top)
{
    //calculate PWM values
    //reverse calculate actual frequency/period
    //from: https://forums.raspberrypi.com/viewtopic.php?t=317593#p1901340
    #define TOP_MAX 65534
    #define DIV_MIN ((0x01 << 4) + 0x0) // 0x01.0
    #define DIV_MAX ((0xFF << 4) + 0xF) // 0xFF.F
    uint32_t clock = 125000000;
    // Calculate a div value for frequency desired
    uint32_t div = (clock << 4) / *freq_hz_value / (TOP_MAX + 1);
    if (div < DIV_MIN) {
        div = DIV_MIN;
    }
    // Determine what period that gives us
    uint32_t period = (clock << 4) / div / *freq_hz_value;
    // We may have had a rounding error when calculating div so it may
    // be lower than it should be, which in turn causes the period to
    // be higher than it should be, higher than can be used. In which
    // case we increase the div until the period becomes usable.
    while ((period > (TOP_MAX+1)) && (div <= DIV_MAX)) {
        period = (clock << 4) / ++div / *freq_hz_value;
    }
    // Check if the result is usable
    if (period <= 1) 
    {
        return 2; //too high
    }
    else if (div > DIV_MAX) 
    {
        return 1; //too low
    } 
    else
    {
        // Determine the top value we will be setting
        uint32_t top = period - 1;
        // Determine what output frequency that will generate
        *pwm_hz_actual = (float)(clock << 4) / div / (top + 1);
        *pwm_ns_actual = ((float)1/(float)(*pwm_hz_actual))*(float)1000000000;
        *pwm_divider=div;
        *pwm_top=top;
        // Report the results
        //printf("Freq = %f\t",         freq);
        //printf("Top = %ld\t",         top);
        //printf("Div = 0x%02lX.%lX\t", div >> 4, div & 0xF);
    }

    return 0;
}

//1000MHZ = 1ns(p)
// 1MHz = 1us(p)
// 1KHz = 1ms(p)
// 1Hz = 1000ms(p)
// f=1/t
// 1s = 1 000ms
// 1s = 1 000 000us
// 1s = 1 000 000 000ns
// f=1/1ms
// f=1/.001s = 1000hz
// t=1/f
// t=1/1KHz
// t=1/1000Hz = .001s = 1ms
void t_to_hz(float* t, uint8_t t_units, float* hz)
{
    uint32_t div;
    switch(t_units)
    {
        case freq_ms:
            div=1000;
            break;
        case freq_us:
            div=1000000;
            break;
        case freq_ns:
            div=1000000000;
            break;

    }

    (*hz)=(float)((float)1/(float)((float)(*t)/(float)div));
}

uint32_t pwm_get_settings(float* pwm_hz_actual, float* duty_user_value) 
{

    float freq_user_value, freq_hz_value, freq_ns_value, pwm_hz_actual_temp, freq_friendly_value, period_friendly_value, pwm_ns_actual;
    uint8_t freq_user_units,freq_friendly_units, period_friendly_units;

    while(1)
    {
        //prompt user for frequency
        prompt_result result;
        ui_prompt_float_units(&result, "Period or frequency (ns, us, ms, Hz, KHz or Mhz)", &freq_user_value, &freq_user_units);

        if(result.exit)
        {
            return 0; //TODO: make sure we can break out....
        }      

        switch(freq_user_units)
        {
            case freq_ms:
            case freq_us:
            case freq_ns:
                t_to_hz(&freq_user_value, freq_user_units, &freq_hz_value);
                break;
            case freq_mhz:
            	freq_hz_value=(float)freq_user_value*(float)1000000;	
                break;
            case freq_khz:
                freq_hz_value=(float)freq_user_value*(float)1000;
                break;
            case freq_hz:
                freq_hz_value=freq_user_value;
                break;
            default: //unknown units, should never get here...
                break;
        }
        freq_ns_value=((float)1/(float)(freq_hz_value))*(float)1000000000;  
        
        freq_display_hz(&freq_hz_value, &freq_friendly_value, &freq_friendly_units);
        freq_display_ns(&freq_ns_value, &period_friendly_value, &period_friendly_units);

        printf("\r\n%sFrequency:%s %s%.3f%s%s = %s%.0f%sHz (%s%.2f%s%s)\r\n%sPeriod:%s %s%.0f%sns (%s%.2f%s%s)\r\n", 
            ui_term_color_notice(),ui_term_color_reset(),ui_term_color_num_float(),
            freq_user_value,ui_term_color_reset(),ui_const_freq_labels[freq_user_units], 
            ui_term_color_num_float(),freq_hz_value, ui_term_color_reset(),
            ui_term_color_num_float(),freq_friendly_value,ui_term_color_reset(),
            ui_const_freq_labels[freq_friendly_units],ui_term_color_notice(),
            ui_term_color_reset(),ui_term_color_num_float(),freq_ns_value,
            ui_term_color_reset(),ui_term_color_num_float(),period_friendly_value,
            ui_term_color_reset(),ui_const_freq_labels[period_friendly_units]			
        );

        if(pwm_freq_find(&freq_hz_value, &pwm_hz_actual_temp, &pwm_ns_actual, &pwm_divider, &pwm_top))
        {
            printf("\r\n%sFrequency invalid:%s 7.46Hz-62.5MHz (134.1ms-16us period)\r\n", ui_term_color_error(), ui_term_color_reset());
            continue;
        }
        freq_display_hz(&pwm_hz_actual_temp, &freq_friendly_value, &freq_friendly_units);
        freq_display_ns(&pwm_ns_actual, &period_friendly_value, &period_friendly_units);

        (*pwm_hz_actual)=pwm_hz_actual_temp;

        printf("\r\n%sActual frequency:%s %s%.0f%sHz (%s%.2f%s%s)\r\n%sActual period:%s %s%.0f%sns (%s%.2f%s%s)\r\n", 
            ui_term_color_warning(),ui_term_color_reset(),ui_term_color_num_float(),
            pwm_hz_actual_temp, ui_term_color_reset(),ui_term_color_num_float(),
            freq_friendly_value,ui_term_color_reset(),ui_const_freq_labels[freq_friendly_units],
            ui_term_color_warning(),ui_term_color_reset(),ui_term_color_num_float(),
            pwm_ns_actual,ui_term_color_reset(),ui_term_color_num_float(),
            period_friendly_value,ui_term_color_reset(),ui_const_freq_labels[period_friendly_units]			
        );

        float duty_ns_value, duty_user_value_temp;
        uint8_t duty_user_units;
 
 pwm_get_duty_cycle:
        //TODO: reinstate ns, us, ms or %
        ui_prompt_float_units(&result, "Duty cycle (%)", &duty_user_value_temp, &duty_user_units);

        if(result.exit)
        {
            return 0; //TODO: make sure we can break out....
        }      

        switch(duty_user_units)
        {
            case freq_percent:
                duty_ns_value=pwm_ns_actual*(duty_user_value_temp/100);
                break;
            case freq_ms: //TODO: implement duty as time
            case freq_us:
            case freq_ns:
            //case freq_mhz: //these make no sense...
            //case freq_khz:
            //case freq_hz:
            default: //unknown units, should never get here...
                printf("\r\nSupport for ns, us or ms duty cycle not yet implemented\r\n");
                goto pwm_get_duty_cycle;
                break;
        }

        //make sure the duty cycle doesn't exceed the period
        if(duty_ns_value>pwm_ns_actual)
        {
            printf("\r\n%sDuty cycle cannot be longer than the period:%s\r\n%s%f%sns > %s%f%sns\r\n",
                ui_term_color_error(),ui_term_color_reset(),ui_term_color_num_float(),
                duty_ns_value,ui_term_color_reset(),ui_term_color_num_float(),
                pwm_ns_actual,ui_term_color_reset()
            );
            goto pwm_get_duty_cycle;

        }

        freq_display_ns(&duty_ns_value, &period_friendly_value, &period_friendly_units);
        printf("\r\n%sDuty cycle:%s %s%.2f%s%s = %s%.0f%sns (%s%.2f%s%s)\r\n", 
            ui_term_color_notice(),ui_term_color_reset(),ui_term_color_num_float(),
            duty_user_value_temp, ui_term_color_reset(),ui_const_freq_labels[duty_user_units], 
            ui_term_color_num_float(),duty_ns_value,ui_term_color_reset(),
            ui_term_color_num_float(),period_friendly_value, ui_term_color_reset(),
            ui_const_freq_labels[period_friendly_units]	
        );     

        //convert to a fraction of the period
        //calculate the actual duty cycle
        //each increment of the pwm is equal to 16ns (at 125MHz clock!!!! TODO: make clock independent)
        //TODO: this needs lots of work.... defo +/-1 errors and less than optimal 
        pwm_duty=((float)(pwm_top)*(float)(duty_user_value_temp/100))+1;
        float duty_ns_actual=((float)pwm_duty/(float)(pwm_top))*pwm_ns_actual;
        //duty_ns_value=pwm_ns_actual*(duty_user_value/100);

        freq_display_ns(&duty_ns_actual, &period_friendly_value, &period_friendly_units);
        printf("%sActual duty cycle:%s %s%.0f%sns (%s%.2f%s%s)\r\n", 
            ui_term_color_notice(),ui_term_color_reset(),ui_term_color_num_float(),
            duty_ns_actual, ui_term_color_reset(),ui_term_color_num_float(),
            period_friendly_value,ui_term_color_reset(),ui_const_freq_labels[period_friendly_units]	
        );
        (*duty_user_value)=duty_user_value_temp;        
        printf("Divider: %d, Period: %d, Duty: %d\r\n", pwm_divider, pwm_top, pwm_duty);  

        return 1;
    }

}