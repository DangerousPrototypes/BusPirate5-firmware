#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "hardware/timer.h"
#include "shift.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "system_monitor.h"
#include "psu.h"
#include "opt_args.h"
#include "display/scope.h"

#define PSU_DAC_VREG_ADJ 0
#define PSU_DAC_CURRENT_ADJ 1
#define PSU_DAC_WRITE 0b000
#define PSU_DAC_READ 0b110

// voltage settings
#define PSU_V_LOW 800  //millivolts
#define PSU_V_HIGH 5000 //millivolts
#define PSU_V_RANGE ((PSU_V_HIGH*10) - (PSU_V_LOW*10))
 
// current settings
#define PSU_I_LOW 0 //mA
#define PSU_I_HIGH 500 //mA
#define PSU_I_RANGE ((PSU_I_HIGH*10000) - (PSU_I_LOW*10000))

bool psu_dac(uint8_t read_write, uint8_t address, uint16_t *value)
{
    uint8_t dac_out_buf[3], dac_in_buf[3];
    
    // Write to DAC
    dac_out_buf[0]=( read_write | (address<<3) ); //AD4:0, C1:0, CMDERR0
    dac_out_buf[1]=((uint8_t)(*value>>8));
    dac_out_buf[2]=(uint8_t)*value;
    uint32_t baud=spi_get_baudrate(BP_SPI_PORT);
    
    spi_busy_wait(true);
    
    //shift_set_clear_wait(0, DAC_CS); // enable the DAC CS using the 74hc595   
    shift_set_clear(0, DAC_CS, false); //busy_wait = false because we're managing the lock here
    spi_set_baudrate(BP_SPI_PORT, 1000 * 1000 * 20); // max 20mhz for our little dac
    spi_write_read_blocking(BP_SPI_PORT, dac_out_buf, dac_in_buf, 3);
    spi_set_baudrate(BP_SPI_PORT, baud);
    shift_set_clear(DAC_CS, 0, false); //busy_wait = false because we're managing the lock here

    spi_busy_wait(false);

    *value=( (dac_in_buf[1]<<8) | dac_in_buf[2] );

    if(!(dac_in_buf[0]&0x01)) //bit 0 = 1, command OK
    { 
        return false; //dac error
    }

    return true;
}

bool psu_dac_read(uint8_t address, uint16_t *value)
{
   return psu_dac(PSU_DAC_READ, address, value);
}

bool psu_dac_write(uint8_t address, uint16_t value)
{
    return psu_dac(PSU_DAC_WRITE, address, &value);
}

static void psu_fuse_reset(){
    //reset current trigger
    shift_set_clear_wait(0, CURRENT_RESET); //low to activate the pnp
    busy_wait_ms(1);
    shift_set_clear_wait(CURRENT_RESET, 0); //high to disable  
}

static void psu_vreg_enable(bool enable){
    if(enable)
    {
        shift_set_clear_wait(0,CURRENT_EN); //low is on (PNP)
    }
    else
    {
        shift_set_clear_wait(CURRENT_EN,0); //high is off
    }
}

bool psu_reset(void)
{
    psu_vreg_enable(false); 
    if( !psu_dac_write(PSU_DAC_VREG_ADJ, system_config.psu_dac_bits_mask) || !psu_dac_write(PSU_DAC_CURRENT_ADJ,127) )
    {
        //ui_term_error_report(T_PSU_DAC_ERROR);
        return false;    
    }
    busy_wait_ms(1);
    psu_fuse_reset(); //reset fuse so it isn't draining current from the opamp
    system_config.psu=0;
    system_pin_claim(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[0]); //change back to vref type pin
    return true;  
} 

// current limit fuse tripped
void psu_irq_callback(uint gpio, uint32_t events)
{
    psu_reset(); //also sets system_config.psu=0
    system_config.psu_current_error=true;
    system_config.psu_error=true;
    system_config.error=true;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[5]);
    gpio_set_irq_enabled(gpio, events, false);
    gpio_acknowledge_irq(gpio, events);   
}


uint32_t psu_set(float volts, float current)
{
    system_config.psu=0;
    system_config.pin_labels[0]=0;
    system_config.pin_changed=0xff;
    system_pin_claim(false, BP_VOUT, 0, 0);

    uint32_t psu_v_per_bit=((PSU_V_RANGE)/system_config.psu_dac_bits_mask);
    uint32_t vset=(uint32_t)((float)volts * 10000);
    vset=vset-(PSU_V_LOW*10);
    vset=vset/psu_v_per_bit;
    if(vset>system_config.psu_dac_bits_mask) vset=system_config.psu_dac_bits_mask;

    system_config.psu_voltage=((vset*psu_v_per_bit)+(PSU_V_LOW*10));

    // inverse for VREG margining
    vset=(system_config.psu_dac_bits_mask-vset);
    system_config.psu_dac_v_set=vset;

    uint32_t psu_i_per_bit= ((PSU_I_RANGE)/system_config.psu_dac_bits_mask);
    uint32_t iset= (uint32_t)((float)current * 10000);
    iset/=psu_i_per_bit;
    system_config.psu_dac_i_set=iset;
    system_config.psu_current_limit=(iset*psu_i_per_bit);

    system_config.psu_error=true;

    // first we start with full current because the inrush will often trip the fuse on low limits
    if( !psu_dac_write(PSU_DAC_VREG_ADJ, (uint16_t)vset) || !psu_dac_write(PSU_DAC_CURRENT_ADJ, (uint16_t)system_config.psu_dac_bits_mask) )
    {
        return 1; 
    }

    psu_fuse_reset();
    psu_vreg_enable(true);
    busy_wait_ms(10);

    //after a delay for inrush, we set the actual limit
    if( !psu_dac_write(PSU_DAC_CURRENT_ADJ, (uint16_t)iset) )
    {
        return 2;    
    }    

    // did the fuse blow?
    // error,  close everything down
    if(!gpio_get(CURRENT_DETECT)){
        psu_reset();
        return 3;   
    }   

    // TODO: is it within 10%?
    // error,  close everything down
    if( hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 100 )
    {
        return 4;           
    }   
    
    //todo: consistent interface to each label of toolbar and LCD, including vref/vout
    system_config.psu=1;
    system_config.psu_error=false;
    system_config.psu_current_error=false;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
    monitor_clear_current(); //reset current so the LCD gets all characters

    gpio_set_irq_enabled_with_callback(CURRENT_DETECT, 0b0001, true, &psu_irq_callback);
    
    return 0;
}



void psu_enable(struct command_attributes *attributes, struct command_response *response)
{
    float volts,current;

    if (scope_running) { // scope is using the analog subsystem
	printf("Can't turn the power supply on when the scope is using the analog subsystem - use the 'ss' command to stop the scope\r\n");
	return;
    }
    system_config.psu=0;
    system_config.pin_labels[0]=0;
    system_config.pin_changed=0xff;
    system_pin_claim(false, BP_VOUT, 0, 0);

    //prompt voltage (float)
    printf("%sPower supply\r\nVolts (0.80V-5.00V)%s", ui_term_color_info(), ui_term_color_reset());  
    prompt_result result;
    ui_prompt_float(&result, 0.8f, 5.0f, 3.3f, true, &volts);
    if(result.exit)
    {
        response->error=true;
        return;
    }

    uint32_t psu_v_per_bit=((PSU_V_RANGE)/system_config.psu_dac_bits_mask);
    uint32_t vset=(uint32_t)((float)volts * 10000);
    vset=vset-(PSU_V_LOW*10);
    vset=vset/psu_v_per_bit;
    if(vset>system_config.psu_dac_bits_mask) vset=system_config.psu_dac_bits_mask;

    system_config.psu_voltage=((vset*psu_v_per_bit)+(PSU_V_LOW*10));

    // actual voltage
    float vact=(float)((float)((vset*psu_v_per_bit)+(PSU_V_LOW*10))/(float)10000);
    printf("%s%1.2f%sV%s requested, closest value: %s%1.2f%sV\r\n", 
        ui_term_color_num_float(), volts, ui_term_color_reset(), ui_term_color_info(),
        ui_term_color_num_float(), vact, ui_term_color_reset()    
    );

    // inverse for VREG margining
    vset=(system_config.psu_dac_bits_mask-vset);
    system_config.psu_dac_v_set=vset;

    //prompt current (float) 
    printf("\r\n%sMaximum current (0mA-250mA)%s", ui_term_color_info(), ui_term_color_reset());
    ui_prompt_float(&result, 0.0f, 500.0f, 100.0f, true, &current);
    if(result.exit)
    {
        response->error=true;
        return;    
    }

    uint32_t psu_i_per_bit= ((PSU_I_RANGE)/system_config.psu_dac_bits_mask);
    uint32_t iset= (uint32_t)((float)current * 10000);
    iset/=psu_i_per_bit;
    system_config.psu_dac_i_set=iset;
    system_config.psu_current_limit=(iset*psu_i_per_bit);
    float iact=(float)((float)iset*psu_i_per_bit)/10000;
    printf("%s%1.1f%smA%s requested, closest value: %s%3.1f%smA\r\n", 
        ui_term_color_num_float(), current, ui_term_color_reset(), ui_term_color_info(),
        ui_term_color_num_float(), iact, ui_term_color_reset()    
    );

    system_config.psu_error=true;

    // first we start with full current because the inrush will often trip the fuse on low limits
    if( !psu_dac_write(PSU_DAC_VREG_ADJ, (uint16_t)vset) || !psu_dac_write(PSU_DAC_CURRENT_ADJ, (uint16_t)system_config.psu_dac_bits_mask) )
    {
        ui_term_error_report(T_PSU_DAC_ERROR);
        response->error=true;
        return;    
    }

    psu_fuse_reset();
    psu_vreg_enable(true);
    busy_wait_ms(10);

    //after a delay for inrush, we set the actual limit
    if( !psu_dac_write(PSU_DAC_CURRENT_ADJ, (uint16_t)iset) )
    {
        ui_term_error_report(T_PSU_DAC_ERROR);
        response->error=true;
        return;    
    }    

    // did the fuse blow?
    // error,  close everything down
    if(!gpio_get(CURRENT_DETECT)){
        ui_term_error_report(T_PSU_CURRENT_LIMIT_ERROR);
        psu_reset();
        response->error=true;
        return;   
    }   

    // TODO: is it within 10%?
    // error,  close everything down
    if( hw_adc_raw[HW_ADC_MUX_VREF_VOUT] < 100 )
    {
        ui_term_error_report(T_PSU_SHORT_ERROR);
        psu_reset();
        response->error=true;
        return;           
    }   

    // is vreg_vout < vref_vout?
    // backflow prevention active
    amux_sweep();
    if( hw_adc_raw[HW_ADC_MUX_VREF_VOUT] > (hw_adc_raw[HW_ADC_MUX_VREG_OUT]+100) ) //+100? TODO: fine tuning
    {
        printf("%s\r\nWarning:\r\n\tBackflow prevention circuit is active\r\n\tVout/Vref voltage is higher than the on-board power supply\r\n\tIs an external supply connected to Vout/Vref?\r\n%s", ui_term_color_warning(), ui_term_color_reset());
    }
    
    //todo: consistent interface to each label of toolbar and LCD, including vref/vout
    system_config.psu=1;
    system_config.psu_error=false;
    system_config.psu_current_error=false;
    system_config.info_bar_changed=true;
    system_pin_claim(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
    monitor_clear_current(); //reset current so the LCD gets all characters
    
    printf("\r\n%s%s:%s%s\r\n",
        ui_term_color_notice(),
        t[T_MODE_POWER_SUPPLY],
        ui_term_color_reset(),
        t[T_MODE_ENABLED]
    );
    
    // print voltage and current
    uint32_t isense=((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000)/4095));
    printf("%s\r\nVreg output: %s%d.%d%sV%s, Vref/Vout pin: %s%d.%d%sV%s, Current sense: %s%d.%d%smA%s\r\n%s", 
    ui_term_color_notice(), 
    ui_term_color_num_float(), ((hw_adc_voltage[HW_ADC_MUX_VREG_OUT])/1000), (((hw_adc_voltage[HW_ADC_MUX_VREG_OUT])%1000)/100), ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_num_float(), ((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])/1000), (((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])%1000)/100), ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_num_float(), (isense/1000), ((isense%1000)/100),ui_term_color_reset(), ui_term_color_notice(),
    ui_term_color_reset()
    );

    gpio_set_irq_enabled_with_callback(CURRENT_DETECT, 0b0001, true, &psu_irq_callback);

    return;
}

void psu_disable(struct command_attributes *attributes, struct command_response *response)
{
    //we disable it before an error just for good measure
    if( !psu_reset() )
    {
        system_config.psu_error=true;
        response->error=true;
        return;    
    }

    printf("%s%s: %s%s\r\n",
        ui_term_color_notice(),
        t[T_MODE_POWER_SUPPLY],
        ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );    
    
    psu_cleanup();
}

//cleanup on mode exit, etc
void psu_cleanup(void)
{
    system_config.psu_error=false;
    system_config.psu=0;
    system_config.info_bar_changed=true;
    monitor_clear_current(); //reset current so the LCD gets all characters next time
}

bool psu_setup(void)
{
    uint16_t value;
        
    system_config.psu=0;
    system_config.psu_error=true;

    psu_vreg_enable(false);

    // the same DAC comes in 8/10/12bit versions, let's try to support them all! (user upgrade opportunity)
    // if we write 0xffff to the DAC output register, the unimplemented bits are discarded
    // when reading back the same register unimplemented bits will be 0s
    //
    // 0xffffff= max DAC output (3.3V) which gives the lowest VREG ouput (~0.8)
    if(!psu_dac_write(PSU_DAC_VREG_ADJ, 0xffff)) //try to set a 16 bit value
    {
        return false;
    }

    if(!psu_dac_read(PSU_DAC_VREG_ADJ, &value)) //read the value back
    {
        return false;
    }
    
    // for now we just save the return for "math" later
    system_config.psu_dac_bits_mask=value;
    switch(value)
    {
        case 0xff: // 8 bits
            system_config.psu_dat_bits_readable=8;
            break;
        case 0x3ff: // 10 bits
            system_config.psu_dat_bits_readable=10; 
            break;       
        case 0xfff: // 12 bits
            system_config.psu_dat_bits_readable=12;
            break;
        default: //error
            system_config.psu_dat_bits_readable=0;
            return false;
            break;
    }

    //small current limit to avoid fuse being half blown out
    if(!psu_dac_write(PSU_DAC_CURRENT_ADJ, 127))
    {
        return false;
    }

    psu_fuse_reset(); //so it isnt floating arouind 1.7v

    if(!gpio_get(CURRENT_DETECT))
    {
        //while(1);
        return false;
    }

    system_config.psu_error=false;
    return true;
}

void psu_init(void){
    gpio_set_function(CURRENT_DETECT, GPIO_FUNC_SIO);
    gpio_set_dir(CURRENT_DETECT, GPIO_IN);
}
