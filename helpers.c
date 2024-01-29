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
#include "ui/ui_format.h"
#include "ui/ui_cmdln.h"
#include "pico/multicore.h"
#include "rgb.h"
//#include "usb_tx.h"
#include "storage.h"
#include "psu.h"
#include "bio.h"
#include "amux.h"
#include "mcu/rp2040.h"

void helpers_selftest(opt_args (*args), struct command_result *res)
{
    helpers_selftest_base();
}

void helpers_selftest_base(void)
{
    #define SELF_TEST_LOW_LIMIT 300
    uint32_t temp1, temp2, fails;


    fails=0;
    
    #ifndef BP_REV8
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
    uint32_t result=psu_set(3.3,100, true);
    if(result)
    {
        printf("PSU ERROR CODE %d\r\n", result);
        fails++;
    }
    else
    {
        printf("OK\r\n");
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
    result=psu_set(3.3,0, false);
    if(result==0)
    {
        printf("OK\r\n");
    }
    else
    {
        printf("PPSU CODE %d, ERROR!\r\n", result);
        fails++;
    }
    psu_reset();
    psu_cleanup();

    //use pullups to trigger current limit, set and reset 
    printf("CURRENT LIMIT TEST: ");
    for(uint8_t pin=0; pin<BIO_MAX_PINS; pin++)
    {
        bio_output(pin);
        bio_put(pin,0);
    }
    //2. Enable wth 0 limit and check the return error code of the PSU
    result=psu_set(3.3,0, true);

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
    psu_reset();
    psu_cleanup();


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
void helpers_bit_order_msb(opt_args (*args), struct command_result *res)
{
    system_config.bit_order=0;
    printf("%s%s:%s %s 0b%s1%s0000000",
        ui_term_color_notice(),t[T_MODE_BITORDER],ui_term_color_reset(),
        t[T_MODE_BITORDER_MSB],	ui_term_color_info(),ui_term_color_reset()
    );
}

void helpers_bit_order_lsb(opt_args (*args), struct command_result *res)
{
    system_config.bit_order=1;
    printf("%s%s:%s %s 0b0000000%s1%s",
        ui_term_color_notice(),	t[T_MODE_BITORDER],	ui_term_color_reset(),
        t[T_MODE_BITORDER_LSB],	ui_term_color_info(), ui_term_color_reset()
    );    
}

void helpers_show_int_formats(opt_args (*args), struct command_result *res)
{
    uint32_t temp=args[0].i;
    //prompt_result result;
    //ui_parse_get_int(&result, &temp);
    uint32_t temp2=system_config.display_format;		// remember old display_format
    system_config.display_format=df_auto; //TODO: this is still a hack...
    
    struct command_attributes attributes;
    
    //determine the effective bits
    attributes.has_dot=true;
    attributes.dot=8;
    uint32_t mask=0x000000ff;
    for(uint8_t i=1; i<4; i++) //4 = 32 bit support TODO: wish we could make this more flexible
    {
        if(temp&(mask<<(i*8)))
        {
            attributes.dot+=8;
        }
    }

    for(uint8_t i=0; i<count_of(ui_const_display_formats); i++) 
    {
        if(i==df_auto ) continue;
        switch(i)
        {
            case df_auto:
                break;
            case df_ascii:
                if(temp>=' '&& temp<='~')
                { 
                    printf("= '%c' ", temp);
                }
                break;
            default:
                printf(" %s=", ui_term_color_reset());
                attributes.number_format=i;
                ui_format_print_number_2(&attributes,&temp);
                break;
        }

    }
    system_config.display_format=temp2;
    //system_config.num_bits=temp3;
}

void helpers_show_int_inverse(opt_args (*args), struct command_result *res)
{
    uint32_t temp=args[0].i;
    //prompt_result result;
    //ui_parse_get_int(&result, &temp);
    uint32_t temp2=system_config.display_format;		// remember old display_format
    system_config.bit_order^=1;
    uint32_t temp3=system_config.num_bits;		// remember old numbits
    system_config.num_bits=32;
    for(uint8_t i=0; i<count_of(ui_const_display_formats); i++)
    {
        if(i==df_ascii||i==df_auto) continue;
        
        printf("|");
        system_config.display_format=i;
        ui_format_print_number(ui_format_bitorder(temp));
    }
    system_config.display_format=temp2;
    system_config.bit_order^=1;
    system_config.num_bits=temp3;
}
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
void helpers_mode_help(opt_args (*args), struct command_result *res)
{
    modes[system_config.mode].protocol_help();
}

void helpers_mode_periodic()
{
    modes[system_config.mode].protocol_periodic();
}

/*
void helpers_mode_write_string(struct command_attributes *attributes, struct command_response *response)
{
    uint32_t i=0;
    bool error=true;
    char c;

    // sanity check! is there a terminating "?
    while(cmdln_try_peek(i,&c))
    {
        if(c=='"')
        {
            error=false;
            break;
        }
        i++;
    }

    if(error)
    {
        printf("Error: string missing terminating '\"'");
        response->error=true;
        return;
    }

    if(!system_config.write_with_read)
    {
        printf("%sTX:%s ", ui_term_color_info(), ui_term_color_reset());
    }

    uint8_t k,b_interval;
    k=b_interval=8;

    attributes->has_string=true; //show ASCII chars
    attributes->number_format=df_hex; //force hex display

    while(i--)
    {
        cmdln_try_remove(&c);

        if(system_config.write_with_read)
        {
            printf("%sTX:%s ", ui_term_color_info(), ui_term_color_reset());
        }
        
        uint32_t j=(uint32_t)c; //TODO: figure out the correct cast
        ui_format_print_number_2(attributes, &j);
        uint32_t received=modes[system_config.mode].protocol_write(ui_format_bitorder(c)); // reshuffle bits if necessary
        
        if(system_config.write_with_read) 
        {
            printf("%s, RX:%s ", ui_term_color_info(), ui_term_color_reset());
            ui_format_print_number_2(attributes, &received);
            printf("\r\n");
        }
        else
        {
            k--;
            printf(" ");
            if(!k)
            {
                printf("\r\n    ");
                k=b_interval;
            } 
        }

    }

    cmdln_try_remove(&c); // consume the final "
}
*/


void helpers_mcu_reset(opt_args (*args), struct command_result *res)
{
 	mcu_reset();
}

void helpers_mcu_jump_to_bootloader(opt_args (*args), struct command_result *res)
{
    mcu_jump_to_bootloader();
}