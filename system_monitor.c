#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "amux.h"

static char voltages_value[HW_PINS-1][4];
static uint32_t voltages_update_mask[3]; 

static uint32_t current_sense;
char current_value[6];
static uint8_t current_update_mask; //bbb.b which digits changed?

//check if each digit of the current value (in ASCII) changed. digits numbered as 432.0
bool monitor_get_current_char(uint8_t digit, char *c)
{
    *c=current_value[digit]; //copy no matter what so we can use it...
    if(current_update_mask & (1u<<digit))
    {
        return true;
    }   

    return false;
}

//if changed, return pointer to value string
bool monitor_get_current_ptr(char **c)
{
    *c=current_value;
    if(current_update_mask)
    {
        return true;
    }

    return false;
    
}

//check if each digit of the voltage value (in ASCII) changed. digits numbered as 2.0
bool monitor_get_voltage_char(uint8_t pin, uint8_t digit, char *c)
{
    *c=voltages_value[pin][digit];
    if(voltages_update_mask[digit] & (1u<<pin))
    {
        return true;
    }
    
    return false;
}

bool monitor_get_voltage_ptr(uint8_t pin, char **c)
{
    *c=voltages_value[pin];
    if(voltages_update_mask[0]||voltages_update_mask[2])
    {
        return true;
    }    
    return false;
}

void monitor_clear_voltage(void)
{
    for(uint8_t i=0; i<count_of(voltages_value); i++)
    {
        voltages_value[i][0]='0';
        voltages_value[i][1]='.';
        voltages_value[i][2]='0';  
        voltages_value[i][3]=0x00;      
    }
}

void monitor_clear_current(void)
{
    current_value[0]='0';  
    current_value[1]='0'; 
    current_value[2]='0'; 
    current_value[3]='.'; 
    current_value[4]='0';   
    current_value[5]=0x00;
}

void monitor_init(void)
{
    voltages_update_mask[0]=0;
    voltages_update_mask[1]=0;
    voltages_update_mask[2]=0;
    monitor_clear_voltage();
    monitor_clear_current();
}

void monitor_reset(void)
{
    voltages_update_mask[0]=0;
    voltages_update_mask[1]=0;
    voltages_update_mask[2]=0;
    current_update_mask=0;
    system_config.pin_changed=0;
    system_config.info_bar_changed=false;
}

void monitor_force_update(void)
{
    voltages_update_mask[0]=0xffffffff;
    voltages_update_mask[1]=0xffffffff;
    voltages_update_mask[2]=0xffffffff;
    current_update_mask=0xff;
    system_config.pin_changed=0xffffffff;
    system_config.info_bar_changed=true;
}

bool monitor_voltage_changed(void)
{
    return (bool)(voltages_update_mask[0]|voltages_update_mask[2]);
}

bool monitor_current_changed(void)
{
    return (bool)current_update_mask;
}

bool monitor(bool current_sense)
{
    char c;

    //TODO hw_adc helper functions - do conversion on request, and cache it????
    {
        extern uint8_t scope_running;
	if (scope_running) 
	    return 0;
    }
    amux_sweep();

    for(uint8_t i=0; i<count_of(voltages_value); i++)
    {
        c=((*hw_pin_voltage_ordered[i])/1000)+0x30; //TODO: really do the +0x30 here????
        if(c!=voltages_value[i][0])
        {
            voltages_value[i][0]=c;
            voltages_update_mask[0]|=(1U<<i);
        }
        c=(((*hw_pin_voltage_ordered[i])%1000)/100)+0x30;
        if(c!=voltages_value[i][2])
        {
            voltages_value[i][2]=c;
            voltages_update_mask[2]|=(1u<<i);
        }  
    }

    if(current_sense)
    {
        uint32_t current_sense_temp=((hw_adc_raw[HW_ADC_CURRENT_SENSE]>>1) * ((500 * 1000)/2048));
        if(current_sense_temp!=current_sense) //value changed, convert to ascii, see which digits changed
        {
            current_sense=current_sense_temp; //TODO: maybe there is more variation here than we want and this hits needlessly
            
            char current_value_temp[6];
            sprintf(current_value_temp, "%03u.%01u", (current_sense_temp/1000), ((current_sense_temp%1000)/100));

            for(uint8_t i=0; i<5; i++)
            {
                if(current_value_temp[i]!=current_value[i]) //if the value of this digit changed, set the mask bit and copy the new value
                {
                    current_update_mask|=(1u<<i);
                    current_value[i]=current_value_temp[i];
                }
            }

        } 
    }

    //TODO: return individually?! global struct? system config?
    return (voltages_update_mask[0]|voltages_update_mask[2]|current_update_mask); //was there an update?
}
