#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "opt_args.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "modes.h"
#include "system_config.h"
#include "ui/ui_cmdln.h"
#include "displays.h"

const struct ui_help_options global_commands[]={
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
//{1,"",T_HELP_SECTION_CAPTURE},
//    {0,"scope",T_HELP_CAPTURE_SCOPE},
//    {0,"logic",T_HELP_CAPTURE_LA},

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

//useful command and mini programs
//{1,"",T_HELP_SECTION_PROGRAMS},
	//{0,"flash", T_HELP_CMD_FLASH},

// enter a mode to use protocols
{1,"",T_HELP_SECTION_MODE},
	{0,"m", 	T_HELP_1_16},
	{0,"(x)/(0)", T_HELP_2_1},
	{0,"= x/| x",T_HELP_1_2},    

//send and receive data in modes using bus syntax
{1,"",T_HELP_SECTION_SYNTAX},
	{0,"[/{", T_HELP_2_3},
	{0,"]/}", T_HELP_2_4},
	{0,"123", T_HELP_2_8},
	{0,"0x123", T_HELP_2_9},
	{0,"0b110", T_HELP_2_10},
	{0,"\"abc\"", T_HELP_2_7},
	{0,"r", T_HELP_2_11},
	{0,"/", T_HELP_SYN_CLOCK_HIGH},
	{0,"\\", T_HELP_SYN_CLOCK_LOW},
	{0,"^", T_HELP_SYN_CLOCK_TICK},
	{0,"-", T_HELP_SYN_DATA_HIGH},
	{0,"_", T_HELP_SYN_DATA_LOW},
	{0,".", T_HELP_SYN_DATA_READ},
	{0,":", T_HELP_2_19},
	{0,".", T_HELP_2_20},	
	{0,"d/D", T_HELP_1_6},
	{0,"a/A/@.x", T_HELP_1_7},	
	{0,"v.x", T_HELP_SYNTAX_ADC},
	{0,">",	T_HELP_GREATER_THAN},    
};

const struct ui_help_options global_commands_more_help[]={
//Get more help  
{1,"",T_HELP_SECTION_HELP}, 
	{0,"?/help",T_HELP_HELP_GENERAL},
    //{0,"hd",    T_HELP_HELP_DISPLAY},
    {0,"-h", T_HELP_HELP_COMMAND},

{1,"",T_HELP_HINT}
};


static const char * const help_usage[]= 
{
    "?|help [mode|display] [-h(elp)]",   
    "Show global commands: ?",
    "Show help and commands for current mode: ? mode",
	"Show help and commands for current display mode: ? display",
};

static const struct ui_help_options help_options[]={
{1,"", T_HELP_HELP}, //command help
    {0,"?/help",T_HELP_SYS_COMMAND }, 
	{0,"mode",T_HELP_SYS_MODE }, 
    {0,"display", T_HELP_SYS_DISPLAY}, 
	{0,"-h", T_HELP_SYS_HELP},
};

void help_display(void){
	if (displays[system_config.display].display_help) {
		displays[system_config.display].display_help();
	} else {
		printf("No display help available for this display mode\r\n");
	}
}

void help_mode(void){
    //ui_help_options(&help_commands[0],count_of(help_commands));
	modes[system_config.mode].protocol_help();
}

void help_global(void){
	//global commands help list
    ui_help_options(&global_commands[0],count_of(global_commands));

	//loop through modes and display available commands
	for(uint32_t i=0; i<count_of(modes); i++){
		if((*modes[i].mode_commands_count)>0){
			//ui_help_mode_commands(modes[i].mode_commands, *modes[i].mode_commands_count);
			//printf("%d\r\n", *modes[i].mode_commands_count);
			ui_help_mode_commands_exec(modes[i].mode_commands, *modes[i].mode_commands_count, modes[i].protocol_name);
		}
	}
	//show more help last
	ui_help_options(&global_commands_more_help[0],count_of(global_commands_more_help));
}

void help_handler(struct command_result *res){
    //check help
    if(ui_help_show(res->help_flag,help_usage,count_of(help_usage), &help_options[0],count_of(help_options) )) return;
	//check mode|global|display
	char action[9];
	cmdln_args_string_by_position(1, sizeof(action), action);
    bool mode = (strcmp(action, "mode")==0);
	bool display = (strcmp(action, "display")==0);
	if(mode){
    	help_mode();
	}else if(display){
		help_display();
	}else{
		help_global();
	}
}
