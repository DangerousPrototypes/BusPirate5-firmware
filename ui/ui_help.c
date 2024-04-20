#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "pirate/amux.h"
#include "ui/ui_term.h"
#include "opt_args.h" //remove?
#include "ui/ui_help.h" 
#include "display/scope.h" 
#include "system_config.h"
#include "bytecode.h"
#include "modes.h"

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

void ui_help_mode_commands_exec(const struct _command_struct *commands, uint32_t count, const char *mode){
    //printf("\r\nAvailable mode commands:\r\n");
    printf("\r\n%s%s%s mode commands:%s\r\n", ui_term_color_prompt(), mode, ui_term_color_info(), ui_term_color_reset());
    for(uint32_t i=0; i<count; i++){
        printf("%s%s%s\t%s%s\r\n", 
            ui_term_color_prompt(), 
            commands[i].command, 
            ui_term_color_info(), 
            commands[i].help_text?t[commands[i].help_text]:"Description not set. Try -h for command help", 
            ui_term_color_reset()
        );
    }
}

void ui_help_mode_commands(const struct _command_struct *commands, uint32_t count){
    ui_help_mode_commands_exec(commands, count, modes[system_config.mode].protocol_name);
}



//true if there is a voltage on out/vref pin
bool ui_help_check_vout_vref(void){
    if (scope_running) // scope is using the analog subsystem
        return true; //can't check, just skip

    if(hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]<790) { //0.8V minimum output allowed to set on internal PSU when reading i could be 0.79
        ui_help_error(T_MODE_NO_VOUT_VREF_ERROR);
        printf("%s%s%s\r\n", ui_term_color_info(), t[T_MODE_NO_VOUT_VREF_HINT], ui_term_color_reset());
        return false;
    }    
    return true;
}

//move to help?
void ui_help_error(uint32_t error){
	printf("\x07\r\n%sError:%s %s\r\n",ui_term_color_error(), ui_term_color_reset(), t[error]);
}
