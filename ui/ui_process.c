#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "modes.h"
#include "hardware/uart.h"
#include "mode/hiz.h"
#include "amux.h"
#include "auxpinfunc.h"
#include "font/font.h"
#include "ui/ui_lcd.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_info.h"
#include "ui/ui_format.h"
#include "ui/ui_init.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_config.h"
#include "ui/ui_mode.h"
#include "pwm.h"
#include "freq.h"
#include "adc.h"
#include "psu.h"
#include "pullups.h"
#include "helpers.h"
#include "ui/ui_cmdln.h"

// const structs are init'd with 0s, we'll make them here and copy in the main loop
const struct command_attributes attributes_empty;
const struct command_response response_empty;
struct command_attributes attributes;
struct command_response response;
struct prompt_result result;
char c;

// statemachine friendly command parsing/processing function
bool ui_process_commands(void)
{
    if(!cmdln_try_peek(0,&c))
    {
        return false;
    }

    if( c<=' ' || c>'~' )
    {
        //out of ascii range
        cmdln_try_discard(1);
    } 
    else if(system_config.mode==HIZ && !commands[(c-0x20)].allow_hiz)
    {
        printf(HiZerror());
        system_config.error=1; 
        cmdln_try_discard(1);
    }
    else
    {
        attributes=attributes_empty;
        response=response_empty;

        attributes.command=c;
        //if number pre-parse it
        if(c>='0' && c<='9')
        {
            ui_parse_get_int(&result, &attributes.value);
            if(result.error)
            {
                system_config.error=1;
            }
            else
            {
                attributes.has_value=true;
                attributes.number_format=result.number_format;
            }

        }
        else
        {   // parsing an int value from the command line sets the pointer to the next value
            // if it's another command, we need to do that manually now to keep the pointer
            // where the next parsing function expects it
            cmdln_try_discard(1);
        }

        if(!system_config.error)
        {
            attributes.has_dot=ui_parse_get_dot(&attributes.dot);
            attributes.has_colon=ui_parse_get_colon(&attributes.colon);
            
            printf("%s", ui_term_color_info()); //TODO: properly color things? maybe only color from here?
            commands[(c-0x20)].command(&attributes,&response);
            
            if(response.error)
            {		
                system_config.error=1;                   
            }
        }
    }
    
    if((c!=' ')&&(c!=0x00)&&(c!=',')) printf("%s\r\n", ui_term_color_reset());

    if(system_config.error)	// something went wrong
    {
        return false; //error, tell sm to change states....
    }

    return true;
}