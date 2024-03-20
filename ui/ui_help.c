#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "pirate/amux.h"
#include "ui/ui_term.h"
#include "opt_args.h" //remove?
#include "ui/ui_help.h" 
#include "display/scope.h" 

// displays the help
void ui_help_options(const struct ui_help_options (*help), uint32_t count){
	for(uint i=0; i<count; i++){
		switch(help[i].help){
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

void ui_help_usage(const char * const flash_usage[], uint32_t count){
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

void ui_help_mode_commands(const struct _command_struct *commands, uint32_t count){
    printf("\r\nAvailable mode commands:\r\n");
    for(uint32_t i=0; i<count; i++){
        printf("%s%s%s\t%s%s\r\n", 
            ui_term_color_prompt(), 
            commands[i].command, 
            ui_term_color_info(), 
            commands[i].help_text?t[commands[i].help_text]:"Unavailable", 
            ui_term_color_reset()
        );
    }
}

//true if there is a voltage on out/vref pin
bool ui_help_check_vout_vref(void){
    if (scope_running) // scope is using the analog subsystem
        return true; //can't check, just skip

    if(amux_read(HW_ADC_MUX_VREF_VOUT)<1200){
        ui_term_error_report(T_MODE_NO_VOUT_VREF_ERROR);
        printf("%s%s%s\r\n", ui_term_color_info(), t[T_MODE_NO_VOUT_VREF_HINT], ui_term_color_reset());
        return false;
    }    
    return true;
}