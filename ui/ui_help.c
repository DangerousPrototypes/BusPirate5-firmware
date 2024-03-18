#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h" //remove?
#include "opt_args.h" //remove?
#include "ui/ui_help.h" //remove?

// displays the help
void ui_help_options(const struct ui_help_options (*help), uint32_t count)
{
	
	for(uint i=0; i<count; i++)
	{
		switch(help[i].help)
		{
            case 1: //heading
                printf("\r\n%s%s%s\r\n", 
                    ui_term_color_info(), 
                    t[help[i].description], 
                    ui_term_color_reset()
                );
                break;
            case 0: //help item
                printf("%s%s%s\t%s%s%s\r\n",
                    ui_term_color_prompt(), help[i].command, ui_term_color_reset(),
                    ui_term_color_info(), t[help[i].description], ui_term_color_reset()
                );
                break;
            case '\n':
                printf("\r\n");
                break;
            default:
                break;
		}
	
	}
}

void ui_help_usage(const char * const flash_usage[], uint32_t count)
{
	printf("usage:\r\n");
	for(uint32_t i=0; i<count; i++)	{
		printf("%s%s%s\r\n", 
			ui_term_color_info(), 
			flash_usage[i], 
			ui_term_color_reset());
	}
}

bool ui_help_show(bool help_flag, const char * const usage[], uint32_t count_of_usage, const struct ui_help_options *options, uint32_t count_of_options){
    if(help_flag){
        ui_help_usage(usage, count_of_usage);
        ui_help_options(&options[0],count_of_options);
        return true;
    }   
    return false;
}
