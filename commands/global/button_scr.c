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

#define BUTTON_FLAG_HIDE_COMMENTS 1u<<0
#define BUTTON_FLAG_EXIT_ON_ERROR 1u<<1
#define BUTTON_FLAG_FILE_CONFIGURED 1u<<2

static uint8_t button_flags=0;
static char button_script_file[32];

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
    if(!cmdln_args_string_by_position(1, sizeof(button_script_file), button_script_file)){
        printf("No script file specified. Try button -h for help\r\n");
        res->error=true;
        return;
    }
    FIL fil;		/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */    
    fr = f_open(&fil, button_script_file, FA_READ);	
    if (fr != FR_OK) {
        printf("'%s' not found\r\n", button_script_file);
        res->error=true;
        return;
    }
    f_close(&fil);  
    button_flags|=BUTTON_FLAG_FILE_CONFIGURED;
    printf("Button script file set to '%s'\r\n", button_script_file);
    if(cmdln_args_find_flag('d'|0x20)) button_flags|=BUTTON_FLAG_HIDE_COMMENTS;
    if(cmdln_args_find_flag('e'|0x20)) button_flags|=BUTTON_FLAG_EXIT_ON_ERROR;
}

// non zero return value indicates error
bool button_exec(void){

    FIL fil;		/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */    
    char file[512];

    if(!(button_flags & BUTTON_FLAG_FILE_CONFIGURED)){
        memcpy(button_script_file, "button.scr", sizeof("button.scr"));
    }

    fr = f_open(&fil, button_script_file, FA_READ);	
    if (fr != FR_OK) {
        printf("'%s' not found. Try button -h for help\r\n", button_script_file);
        return true;
    }

    printf("\r\n");
    while (f_gets(file, sizeof(file), &fil)) {
        if(file[0]=='#'){ //comment line TODO: make a little more robust against leading whitespace
            if(!(button_flags & BUTTON_FLAG_HIDE_COMMENTS))printf("%s%s%s\r",ui_term_color_info(), file, ui_term_color_reset());
        }else{   
            cmdln_next_buf_pos();
            if(system_config.subprotocol_name){
                printf("%s%s-(%s)>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, system_config.subprotocol_name, ui_term_color_reset());
            }else{
                printf("%s%s>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, ui_term_color_reset());
            }
            for(uint32_t i=0; i<sizeof(file); i++){
                if(file[i]=='\r' || file[i]=='\n' || file[i]=='\0') break;
                rx_fifo_add(&file[i]); 
            }
            while(ui_term_get_user_input()!=0); //nothing left to shove in the command prompt
            printf("\r\n");
            bool error=ui_process_commands();
            if(error && (button_flags & BUTTON_FLAG_EXIT_ON_ERROR)){
                return true;
            }
        }
    }
    printf("\r\n");
    /* Close the file */
    f_close(&fil);  
    return false;
}