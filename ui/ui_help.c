#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_term.h"
#include "storage.h"
#include "freq.h"
#include "ui/ui_const.h"
#include "ui/ui_help.h"
#include "mcu/rp2040.h"
#include "amux.h"
#include "mem.h" //big buffer owner defines



const struct ui_info_help help_commands[]=
{
{1,"", T_HELP_SECTION_IO}, //work with pins, input, output measurement
    {0,"w/W", 	T_HELP_1_21}, //note that pin functions need power on the buffer
    {0,"a/A/@ x",T_HELP_COMMAND_AUX},
    {0,"f x/F x", T_HELP_1_11},
    {0,"f/F", 	T_HELP_1_23},
    {0,"g x/G",     T_HELP_1_12},
    {0,"p/P", 	T_HELP_1_18},
    {0,"v x/V x", T_HELP_1_22},
    {0,"v/V", 	T_HELP_1_10},

//measure analog and digital signals
{1,"",T_HELP_SECTION_CAPTURE},
    {0,"scope",T_HELP_CAPTURE_SCOPE},
    {0,"logic",T_HELP_CAPTURE_LA},

//configure the terminal, LEDs, display and mode
{1,"",T_HELP_SECTION_CONFIGURE},
	{0,"c", 	T_HELP_1_9},
	{0,"d", 	T_HELP_COMMAND_DISPLAY},
	{0,"o", 	T_HELP_1_17},
	{0,"l/L", 	T_HELP_1_15},

//restart, firmware updates and diagnostic
{1,"",T_HELP_SECTION_SYSTEM},
	{0,"i", 	T_HELP_1_14},
	{0,"#", 	T_HELP_1_4},
	{0,"$", 	T_HELP_1_5},   
	{0,"~", 	T_HELP_1_3},     

//list files and navigate the storage
{1,"",T_HELP_SECTION_FILES},
	{0,"ls", T_HELP_CMD_LS},
	{0,"cd", T_HELP_CMD_CD},
	{0,"mkdir", T_HELP_CMD_MKDIR},
	{0,"rm", T_HELP_CMD_RM},
	{0,"cat", T_HELP_CMD_CAT},
    {0,"hex",T_HELP_CMD_HEX},//     Print HEX file contents
	{0,"pause", T_HELP_CMD_PAUSE},
	{0, "format", T_HELP_CMD_FORMAT},

// enter a mode to use protocols
{1,"",T_HELP_SECTION_MODE},
	{0,"m", 	T_HELP_1_16},
	{0,"(x)/(0)", T_HELP_2_1},
	{0,"= x/| x",T_HELP_1_2},    

//send and receive data in modes using bus syntax
{1,"",T_HELP_SECTION_SYNTAX},
	{0,"[", T_HELP_2_3},
	{0,"{", T_HELP_2_5},	
	{0,"]/}", T_HELP_2_4},
	{0,"123", T_HELP_2_8},
	{0,"0x123", T_HELP_2_9},
	{0,"0b110", T_HELP_2_10},
	{0,"\"abc\"", T_HELP_2_7},
	{0,"r", T_HELP_2_11},
	{0,":", T_HELP_2_19},
	{0,".", T_HELP_2_20},	
	{0,"d/D", T_HELP_1_6},
	{0,"a/A/@.x", T_HELP_1_7},	
	{0,"v.x", T_HELP_SYNTAX_ADC},
	{0,">",	T_HELP_GREATER_THAN},    

//Get more help  
{1,"",T_HELP_SECTION_HELP}, 
	{0,"?",    T_HELP_HELP_GENERAL},
    {0,"h",    T_HELP_HELP_MODE},
    {0,"hd",    T_HELP_HELP_DISPLAY},
    {0,"", T_HELP_HELP_COMMAND},

{1,"",T_HELP_HINT}
};

void ui_help_print_args(struct command_result *res)
{
    ui_help_options(&help_commands[0],count_of(help_commands));
}

// displays the help
void ui_help_options(const struct ui_info_help (*help), uint32_t count)
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
	for(uint32_t i=0; i<count; i++)
	{
		printf("%s%s%s\r\n", 
			ui_term_color_info(), 
			flash_usage[i], 
			ui_term_color_reset()
		);
	}
}
