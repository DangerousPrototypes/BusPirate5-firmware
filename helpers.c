#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "command_attributes.h"
#include "helpers.h"
#include "bytecode.h"
#include "commands.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "modes.h"
#include "displays.h"
#include "ui/ui_format.h"
#include "ui/ui_cmdln.h"
#include "pico/multicore.h"
#include "rgb.h"
//#include "usb_tx.h"
#include "storage.h"
#include "pirate/psu.h"
#include "bio.h"
#include "amux.h"
#include "display/scope.h"
#include "mode/logicanalyzer.h"
#include "usb_rx.h"

void helpers_selftest(struct command_result *res)
{
    helpers_selftest_base();
}

void helpers_selftest_base(void)
{
    #define SELF_TEST_LOW_LIMIT 300
    uint32_t temp1, temp2, fails;

    if(system_config.mode!=0) //only allow selftest in HiZ mode
    {
        printf("Selftest only available in HiZ mode\r\n");
        return;
    }

    fails=0;
    
    #if BP5_REV >= 10
    uint32_t value;
    struct prompt_result presult;     
    if(!system_config.storage_available)
    {
        printf("No file system!\r\nFormat the Bus Pirate NAND flash?\r\nALL DATA WILL BE DESTROYED.\r\n y/n> ");
        cmdln_next_buf_pos();
        while(1)
        {
            ui_prompt_vt100_mode(&presult, &value);
            if(presult.success) break;
        }
        
        printf("\r\n\r\n");
        
        if(value=='y')
        {

            if(!storage_format_base())
            {
                printf("FORMAT NAND FLASH: ERROR! 不好\r\n\r\n");
                fails++;                              
            }
            else
            {
                printf("FORMAT NAND FLASH: OK\r\n\r\n");
            }
        }    
    }
    #endif    

    if (scope_running) { // scope is using the analog subsystem
	printf("Can't self test when the scope is using the analog subsystem - use the 'd 1' command to switch to the default display\r\n");
	return;
    }
    printf("SELF TEST STARTING\r\nDISABLE IRQ: ");
    multicore_fifo_push_blocking(0xf0);
    while(multicore_fifo_pop_blocking()!=0xf0);
    busy_wait_ms(500);
    printf("OK\r\n");

    //init pins
    bio_init();

    //USB/VOLTAGE/ADC/AMUX test: read the (USB) power supply
    amux_sweep();

    printf("ADC SUBSYSTEM: VUSB ");
    if(hw_adc_voltage[HW_ADC_MUX_VUSB]< (4.75 * 1000))
    {
        printf("NOT DETECTED (%1.2fV). ERROR!\r\n", (float)(hw_adc_voltage[HW_ADC_MUX_VUSB]/(float)1000));
        fails++;
    }
    else
    {
        printf(" %1.2fV OK\r\n", (float)(hw_adc_voltage[HW_ADC_MUX_VUSB]/(float)1000));
    }

    /*//DAC test: was DAC detected?
    printf("DAC READ/WRITE: ");
    if(psu_setup())
    {
        printf("OK\r\n");
    }
    else
    {
        printf("FAILED. ERROR!\r\n");
    }*/


    //TF flash card check: was TF flash card detected?
    printf("FLASH STORAGE: ");
    if(storage_mount())
    {
        printf("OK\r\n");
    }
    else
    {
        printf("NOT DETECTED. ERROR!\r\n");
        fails++;
    }  

    //psu test
    //psu to 1.8, 2.5, 3.3, 5.0 volt test
    printf("PSU ENABLE: ");
    uint32_t result=psu_enable(3.3,100, true);
    if(result)
    {
        printf("PSU ERROR CODE %d\r\n", result);
        fails++;
    }
    else
    {
        printf("OK\r\n");
    }

    //detect REV10 failure of op-amp
    printf("VREG==VOUT: ");
    amux_sweep();
    //at 3.3v, the VREGout should be close  to VOUT/VREF
    if(hw_adc_voltage[HW_ADC_MUX_VREG_OUT]>hw_adc_voltage[HW_ADC_MUX_VREF_VOUT])
    {
        if(hw_adc_voltage[HW_ADC_MUX_VREG_OUT]-hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] > 100)
        {
            printf(" %d > %d ERROR!!\r\n",hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] );
            fails++;
        }  
        else
        {
            printf(" %d = %d OK\r\n",hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] );
        } 
    }
    else
    {
        if(hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]-hw_adc_voltage[HW_ADC_MUX_VREG_OUT] > 100)
        {
            printf(" %d < %d ERROR!!\r\n",hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] );
            fails++;
        }    
        else
        {
            printf(" %d = %d OK\r\n",hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] );
        }      
    }

    printf("BIO FLOAT TEST (SHOULD BE 0/<0.%dV)\r\n", SELF_TEST_LOW_LIMIT/10);
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        //read pin input (should be low)
        amux_sweep();
        temp1=bio_get(pin);

        printf("BIO%d FLOAT: %d/%1.2fV ", pin, temp1, (float)(*hw_pin_voltage_ordered[pin+1]/(float)1000));

        if(temp1 || ((*hw_pin_voltage_ordered[pin+1])>SELF_TEST_LOW_LIMIT) )
        {
            printf("ERROR!\r\n");
            fails++;
        }
        else 
        {
            printf("OK\r\n");
        }
    }

    printf("BIO HIGH TEST (SHOULD BE >3.00V)\r\n");
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        bio_output(pin);
        bio_put(pin,1);
        busy_wait_ms(1); //give it some time

        //read pin ADC, should be ~3.3v
        amux_sweep();
        printf("BIO%d HIGH: %1.2fV ", pin, (float)(*hw_pin_voltage_ordered[pin+1]/(float)1000));
        if(*hw_pin_voltage_ordered[pin+1]<3*1000)
        {
            printf("ERROR!\r\n");
            fails++;
        }
        else
        {
            printf("OK\r\n");
        }

        //check other pins for possible shorts
        for(uint8_t i=0; i<BIO_MAX_PINS; i++)
        {
            if(pin==i) continue;
            temp1=bio_get(i);
            if( temp1 || *hw_pin_voltage_ordered[i+1]>SELF_TEST_LOW_LIMIT)
            {
                printf("BIO%d SHORT->BIO%d (%d/%1.2fV): ERROR!\r\n", pin, i, temp1, (float)(*hw_pin_voltage_ordered[i+1]/(float)1000));
                fails++;
            }

        }

        bio_input(pin);

    }

    printf("BIO LOW TEST (SHOULD BE <0.%dV)\r\n", SELF_TEST_LOW_LIMIT/10);
    //start with all pins high, ground one by one
    for(uint8_t i=0; i<BIO_MAX_PINS; i++)
    {
        bio_output(i);
        bio_put(i,1);   
    }
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        bio_put(pin,0);
        busy_wait_ms(1); //give it some time

        //read pin ADC, should be ~3.3v
        amux_sweep();
        printf("BIO%d LOW: %1.2fV ", pin, (float)(*hw_pin_voltage_ordered[pin+1]/(float)1000));
        if(*hw_pin_voltage_ordered[pin+1]>SELF_TEST_LOW_LIMIT)
        {
            printf("ERROR!\r\n");
            fails++;
        }
        else
        {
            printf("OK\r\n");
        }

        //check other pins for possible shorts
        for(uint8_t i=0; i<BIO_MAX_PINS; i++)
        {
            if(pin==i) continue;
            if(*hw_pin_voltage_ordered[i+1]<3*1000)
            {
                printf("BIO%d SHORT->BIO%d (%1.2fV): ERROR!\r\n", pin, i, (float)(*hw_pin_voltage_ordered[i+1]/(float)1000));
                fails++;
            }

        }

        bio_put(pin,1);

    } 

    bio_init();

    printf("BIO PULL-UP HIGH TEST (SHOULD BE 1/>3.00V)\r\n");
    HW_BIO_PULLUP_ENABLE();

    //start with all pins grounded, then float one by one
    for(uint8_t i=0; i<BIO_MAX_PINS; i++)
    {
        bio_output(i);
        bio_put(i,0);   
    }
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        bio_input(pin); //let float high
        busy_wait_ms(5); //give it some time


        //read pin input (should be low)
        amux_sweep();
        temp1=bio_get(pin);

        printf("BIO%d PU-HIGH: %d/%1.2fV ", pin, temp1, (float)(*hw_pin_voltage_ordered[pin+1]/(float)1000));

        if(!temp1 || ((*hw_pin_voltage_ordered[pin+1])<3 * 1000) )
        {
            printf("ERROR!\r\n");
            fails++;
        }
        else 
        {
            printf("OK\r\n");
        }        

        //check other pins for possible shorts
        for(uint8_t i=0; i<BIO_MAX_PINS; i++)
        {
            if(pin==i) continue;
            if(*hw_pin_voltage_ordered[i+1]>SELF_TEST_LOW_LIMIT)
            {
                printf("BIO%d SHORT->BIO%d (%1.2fV): ERROR!\r\n", pin, i, (float)(*hw_pin_voltage_ordered[i+1]/(float)1000));
                fails++;
            }

        }

        bio_output(pin);
        bio_put(pin,0);   

    } 

    bio_init();

    printf("BIO PULL-UP LOW TEST (SHOULD BE <0.%dV)\r\n",SELF_TEST_LOW_LIMIT/10);
    HW_BIO_PULLUP_ENABLE();   
    //start with all pins floating, then ground one by one
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        bio_output(pin); //let float high
        bio_put(pin,0);
        busy_wait_ms(5); //give it some time

        //read pin input (should be high)
        amux_sweep();

        printf("BIO%d PU-LOW: %1.2fV ", pin, (float)(*hw_pin_voltage_ordered[pin+1]/(float)1000));

        if(((*hw_pin_voltage_ordered[pin+1])>SELF_TEST_LOW_LIMIT) )
        {
            printf("ERROR!\r\n");
            fails++;
        }
        else 
        {
            printf("OK\r\n");
        }        

        //check other pins for possible shorts
        for(uint8_t i=0; i<BIO_MAX_PINS; i++)
        {
            if(pin==i) continue;
            temp1=bio_get(i);
            if(temp1==0 || *hw_pin_voltage_ordered[i+1]<3*1000)
            {
                printf("BIO%d SHORT->BIO%d (%d/%1.2fV): ERROR!\r\n", pin, i, temp1, (float)(*hw_pin_voltage_ordered[i+1]/(float)1000));
                fails++;
            }

        }

        bio_input(pin);

    } 

    //1. Test with current override
    printf("CURRENT OVERRIDE: ");
    result=psu_enable(3.3,0, false);
    if(result==0)
    {
        printf("OK\r\n");
    }
    else
    {
        printf("PPSU CODE %d, ERROR!\r\n", result);
        fails++;
    }
    psu_disable();

    //use pullups to trigger current limit, set and reset 
    printf("CURRENT LIMIT TEST: ");
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        bio_output(pin);
        bio_put(pin,0);
    }
    //2. Enable wth 0 limit and check the return error code of the PSU
    result=psu_enable(3.3,0, true);

    if(result==3)
    {
        printf("OK\r\n");
    }
    else
    {
        uint i;
        for(i=0; i<5; i++)
        {
            amux_sweep();
            printf("PPSU CODE %d, ADC: %d, ERROR!\r\n", result, hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT]);
            busy_wait_ms(200);
        }
        fails++;
    }


/*
    result=2000;
    
    while(result) //detect the interrupt
    {
        amux_sweep();
        if(hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT] < 100)
        {
            //success fuse blew
            system_config.psu_current_error=true;
            break;
        }
        busy_wait_ms(1);
        result--;
    }
    if(system_config.psu_current_error)
    {
        printf("OK\r\n");
        system_config.psu_current_error=false;
    }
    else
    {
        printf("ERROR!\r\n");
        fails++;
    }
    */

    HW_BIO_PULLUP_DISABLE();   
    bio_init();
    psu_disable();

    //prompt to push button
    gpio_set_function(EXT1, GPIO_FUNC_SIO);
    gpio_set_dir(EXT1, GPIO_IN);
    gpio_pull_down(EXT1);
    printf("PUSH BUTTON TO COMPLETE: ");
    result=2000;
    while(!gpio_get(EXT1))
    {
        //busy_wait_ms(1);
        //result--;
        //if(!result) break;
    }
    printf("OK\r\n");

    if(fails)
    {
        printf("\r\nERRORS: %d\r\nFAIL! :(\r\n", fails);
    }
    else
    {
        printf("\r\n\r\nPASS :)\r\n");
    }

    system_config.psu_current_error=false;
    system_config.psu_error=false;
    system_config.error=false;


    //enable system interrupts
    multicore_fifo_push_blocking(0xf1);
    while(multicore_fifo_pop_blocking()!=0xf1);

}
/*/
void helpers_numbits(struct command_attributes *attributes, struct command_response *response)
{
    if(attributes->has_dot)
    {
        system_config.num_bits=attributes->dot;
        printf("Default number of bits: %u",system_config.num_bits);
        
    }
}
*/
/*/
void helpers_mode_macro(struct command_attributes *attributes, struct command_response *response)
{
    uint32_t temp;

    prompt_result result;
    ui_parse_get_macro(&result, &temp);   
    if(result.success)
    {
        modes[system_config.mode].protocol_macro(temp);
    }
    else
    {
        response->error=true;
        printf(t[T_MODE_ERROR_PARSING_MACRO]);
    }    
}
*/
/*
void helpers_mode_help(struct command_result *res)
{
    modes[system_config.mode].protocol_help();
}
*/

void helpers_display_help(struct command_result *res)
{
    if (displays[system_config.display].display_help) {
        displays[system_config.display].display_help();
    } else {
        printf("No display help available for this display mode\r\n");
    }
}

void helpers_pause_args(struct command_result *res)
{
    char c;
    printf("%s\r\n", t[T_PRESS_ANY_KEY]);
    while(!rx_fifo_try_get(&c) && !system_config.error)
    {
        busy_wait_ms(1);
    }   
}

void helpers_mode_periodic()
{
    displays[system_config.display].display_periodic();
    modes[system_config.mode].protocol_periodic();
    //we need an array with claim/unclaim slots in an array of active utilities
    la_periodic();
}

