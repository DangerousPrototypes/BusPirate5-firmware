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

static const char * const usage[]= {
    "tutorial <file> [-h(elp)]",
    "Run tutorial: tutorial example.tut",
};

static const struct ui_help_options options[]= {
//{1,"", T_HELP_FLASH}, //flash command help
//    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
};


//tutorial load, display
//check file flag, check file exists
//load file
//print comments lines starting with #
//inset data lines into the command queue
// run syntax parser
void tutorial_handler(struct command_result *res){
    //check help
   	if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;

    FIL fil;		/* File object needed for each open file */
    FRESULT fr;     /* FatFs return code */    
    char file[512];
    char location[32];
    cmdln_args_string_by_position(1, sizeof(location), location);    
    fr = f_open(&fil, location, FA_READ);	
    if (fr != FR_OK) {
        storage_file_error(fr);
        res->error=true;
        return;
    }
    /* Read every line and display it */
    //char c;
    //while (cmdln_try_remove(&c));    
    while (f_gets(file, sizeof(file), &fil)) {
        if(file[0]=='#'){ //comment line TODO: make a little more robust against leading whitespace
            printf("%s%s%s\r",ui_term_color_prompt(), file, ui_term_color_reset());
        }else{
            cmdln_next_buf_pos();
            if(system_config.subprotocol_name){
                printf("%s%s-(%s)>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, system_config.subprotocol_name, ui_term_color_reset());
            }else{
                printf("%s%s>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, ui_term_color_reset());
            }
            for(uint32_t i=0; i<sizeof(file); i++){
                if(file[i]=='\r' || file[i]=='\n' || file[i]=='\0') break;
                //rx_fifo_add(&file[i]); 
                cmdln_try_add(&file[i]);
                tx_fifo_put(&file[i]);
            }
            cmdln_try_add('\0');
            //printf("Process syntax\r\n");
            //TODO: first time show instructions
            //printf("<enter> to continue, x to exit\r\n");
            //todo: pause for user, x for exit
                while(ui_term_get_user_input()!=0xff);
                printf("\r\n");
                bool error=ui_process_commands();
            //printf("Error: %d\r\n", error);
            //return ui_process_syntax(); //I think we're going to run into issues with the ui_process loop if &&||; are used....
        }
    }
    printf("\r\n");
    /* Close the file */
    f_close(&fil);  
}