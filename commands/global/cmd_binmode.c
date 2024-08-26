#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "opt_args.h" // File system related
#include "fatfs/ff.h" // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h" // This file is needed for the command line parsing functions
//#include "ui/ui_prompt.h" // User prompts and menu system
//#include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "msc_disk.h"
#include "binmode/binmodes.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"


// This array of strings is used to display help USAGE examples for the dummy command
static const char * const usage[]= {
    "binmode",
    "Configure the active binary mode: binmode",
};


static const struct ui_help_options options[]= 
{
 
};

bool binmode_prompt_menu(const struct ui_prompt* menu){
    for(uint8_t i=0; i<count_of(binmodes); i++){
        printf(" %d. %s\r\n", i+1, binmodes[i].binmode_name);
    }
}

bool binmode_check_range(const struct ui_prompt *menu, uint32_t *value){
    if((*value > 0) && (*value < (count_of(binmodes)+1))){
        return true;
    }
    return false;
}

void cmd_binmode_handler(struct command_result *res){
    // we can use the ui_help_show function to display the help text we configured above
   	if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;

    static const struct ui_prompt_config binmode_menu_config={
        false,false,false,true,
        &binmode_prompt_menu,  
        &ui_prompt_prompt_ordered_list, 
        &binmode_check_range
    };

    static const struct ui_prompt binmode_menu={
        T_CONFIG_BINMODE_SELECT,
        0,0,0, 
        0,0,0,
        0, &binmode_menu_config
    };    

    prompt_result result;
    uint32_t binmode_number;
    ui_prompt_uint32(&result, &binmode_menu, &binmode_number);

    if(result.exit){
        (*res).error=true;
        return;
    }

    binmode_number--;
    binmode_cleanup();
    printf("\r\n%sBinmode selected:%s %s\r\n", ui_term_color_info(), ui_term_color_reset(), binmodes[binmode_number].binmode_name);
    system_config.binmode_select = binmode_number;
    binmode_setup();

    //optional: save system config options?

}