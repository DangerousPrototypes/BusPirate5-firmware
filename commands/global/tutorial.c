#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "ui/ui_term.h"    // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "commands/global/script.h"

//NOTE: tutorial is basically a simplified alias to script
static const char * const usage[]= {
    "tutorial <file> [-h(elp)]",
    "Run tutorial: tutorial example.tut",
    "",
    "Tutorial files:",
    "Tutorial files are stored in text files with the .tut extension",
    "Lines starting with '#' are comments",
    "Other lines are inserted into the command prompt",
    "Example:",
    "# This is my example tutorial file",
    "# The 'pause' command waits for any key press",
    "pause",
    "# Did it pause?",
};

static const struct ui_help_options options[]= {
//{1,"", T_HELP_FLASH}, //flash command help
//    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
};

void tutorial_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    if(script_exec(true, true, true, false)) res->error=true;
}