#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"
#include "opt_args.h" // File system related
#include "fatfs/ff.h" // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h" // This file is needed for the command line parsing functions
#include "ui/ui_term.h"    // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "commands/global/script.h"
#include "bulong_scr.h"

static const char * const usage[]= {
    "bulong <file> [-d (hiDe comments)] [-e(xit on error)] [-h(elp)]",
    "Assign script file to button (long press): bulong example.scr",
    "Exit script on error option: bulong example.scr -e",
    "Default (long press) script file is 'bulong.scr' in the root directory",
};

static const struct ui_help_options options[]= {
//{1,"", T_HELP_FLASH}, //flash command help
//    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
};

void bulong_scr_handler(struct command_result *res){
   //check help
   	if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    button_flags=0;
    // char script_file[BP_FILENAME_MAX + 1];
    if(!cmdln_args_string_by_position(1, sizeof(button_long_script_file), button_long_script_file)){
        printf("No (long press) script file specified. Try bulong -h for help\r\n");
        res->error=true;
        return;
    }
    FIL fil;		/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */    
    fr = f_open(&fil, button_script_file, FA_READ);	
    if (fr != FR_OK) {
        printf("'%s' not found\r\n", button_long_script_file);
        res->error=true;
        return;
    }
    f_close(&fil);  
    button_flags|=BUTTON_LONG_FLAG_FILE_CONFIGURED;
    printf("Button (long press) script file set to '%s'\r\n", button_long_script_file);
    if(cmdln_args_find_flag('d'|0x20)) button_flags|=BUTTON_FLAG_HIDE_COMMENTS;
    if(cmdln_args_find_flag('e'|0x20)) button_flags|=BUTTON_FLAG_EXIT_ON_ERROR;
}

