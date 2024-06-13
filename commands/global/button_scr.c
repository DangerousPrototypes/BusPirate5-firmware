#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
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
#include "button_scr.h"

#define BUTTON_FLAG_HIDE_COMMENTS 1u<<0
#define BUTTON_FLAG_EXIT_ON_ERROR 1u<<1
#define BUTTON_FLAG_FILE_CONFIGURED 1u<<2
#define BUTTON_LONG_FLAG_FILE_CONFIGURED 1u<<3

static uint8_t button_flags=0;
static const char *button_script_file = "button.scr";
static const char *button_long_script_file = "bulong.scr";

static const char * const usage[]= {
    "button <file> [-d (hiDe comments)] [-e(xit on error)] [-h(elp)]",
    "Assign script file to button: button example.scr",
    "Exit script on error option: button example.scr -e",
    "Default script file is 'button.scr' in the root directory",
};

static const struct ui_help_options options[]= {
//{1,"", T_HELP_FLASH}, //flash command help
//    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
};

void button_scr_handler(struct command_result *res){
   //check help
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
    button_flags=0;
    char script_file[32];
    if(!cmdln_args_string_by_position(1, sizeof(script_file), script_file)){
        printf("No script file specified. Try button -h for help\r\n");
        res->error=true;
        return;
    }
    FIL fil;        /* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */    
    fr = f_open(&fil, script_file, FA_READ);    
    if (fr != FR_OK) {
        printf("'%s' not found\r\n", script_file);
        res->error=true;
        return;
    }
    f_close(&fil);  
    button_flags|=BUTTON_FLAG_FILE_CONFIGURED;
    printf("Button script file set to '%s'\r\n", script_file);
    if(cmdln_args_find_flag('d'|0x20)) button_flags|=BUTTON_FLAG_HIDE_COMMENTS;
    if(cmdln_args_find_flag('e'|0x20)) button_flags|=BUTTON_FLAG_EXIT_ON_ERROR;
}

// non zero return value indicates error
bool button_exec(enum button_codes button_code){
    const char *script_file;

    if(button_code == BP_BUTT_LONG_PRESS){
        if(!(button_flags & BUTTON_LONG_FLAG_FILE_CONFIGURED)){
            button_flags |= BUTTON_LONG_FLAG_FILE_CONFIGURED;
            printf("Using long default '%s'\r\n", button_long_script_file);
        }
        script_file = button_long_script_file;
    } else {
        if(!(button_flags & BUTTON_FLAG_FILE_CONFIGURED)){
            button_flags |= BUTTON_FLAG_FILE_CONFIGURED;
            printf("Using default '%s'\r\n", button_script_file);
        }
        script_file = button_script_file;
    }

    printf("\r\n");

    if(script_exec((char *)script_file, false, !(button_flags & BUTTON_FLAG_HIDE_COMMENTS), false, (button_flags & BUTTON_FLAG_EXIT_ON_ERROR))){
        printf("\r\nError in script file '%s'. Try button -h for help\r\n", script_file);
        return true;
    }
    return false;

}
