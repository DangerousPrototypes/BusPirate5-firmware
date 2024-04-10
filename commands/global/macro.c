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

static bool exec_macro_id(uint8_t id);
void disk_show_macro_file(const char *location);
void disk_get_line_id(const char *location, uint8_t id, char *line, int max_len);

static const char * const usage[]= {
    "macro <#>\r\n\t[-f <file>] [-a] [-l] [-h(elp)]",
    "Load macros: macro -f <file>",
    "List macros: macro -l",
    "Run macro 1: macro 1",
    "Macro 1 help: macro 1 -h",
    "Macro system help: macro -h",
    "List macro files: macro -a",
    "",
    "Macro files:",
    " Macros are stored in text files with the .mcr extension",
    " Lines starting with '#' are comments",
    " Lines starting with '#!' are macro usage instructions",
    " Every macro line includes an id (>0), a separator ':', and bus syntax",
    "Example:",
    " # This is my example macro file",
    " #! Read 5 bytes from an I2C EEPROM",
    " 1:[0xa0 0][0xa1 r:5]",
};

static const struct ui_help_options options[]= {
//{1,"", T_HELP_FLASH}, //flash command help
//    {0,"-f",T_HELP_FLASH_FILE_FLAG}, //file to read/write/verify    
};

#define MACRO_FNAME_LEN  13
static char macro_file[MACRO_FNAME_LEN];

void macro_handler(struct command_result *res){
    uint32_t value;
    //char file[13];

    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;

    //list of mcr files
    if(cmdln_args_find_flag('a'|0x20)){
        printf("Available macro files:\r\n");
        storage_ls("", "mcr"); //disk ls should be integrated with existing list function???
        return;
    }

    //file to load?
    command_var_t arg;
    bool file_flag = cmdln_args_find_flag_string('f'|0x20, &arg, sizeof(macro_file), macro_file);
    if(file_flag ){
        printf("Set current file macro to: '%s'\r\n", macro_file);
        return;
    }

    //list macros
    bool list_flag = cmdln_args_find_flag('l'|0x20);
    if (list_flag){
        //is a macro file loaded? 
        //list macros
        printf("'%s' available macros:\r\n\r\n", macro_file);
        disk_show_macro_file(macro_file); // TODO: manage errors
        return;  
    }    
    
    //run macro
    
    if(cmdln_args_uint32_by_position(1, &value)){
        //is a macro file loaded?
        //does this macro exist?
        exec_macro_id((uint8_t)value); //has return value to flag error
        return;
    }

    printf("Nothing to do. Use -h for help.\r\n");
}

static bool exec_macro_id(uint8_t id)
{
    char line[512];
    printf("Exec macro id: %u\r\n", id);

    disk_get_line_id(macro_file, id, line, sizeof(line));
    if (!line[0]) {
        printf("Macro not fund\r\n");
        return true;
    }

    char *m = line;
    // Skip id and separator
    // 123:MACRO
    while (*m && *m!=':')
        m++;
    if (*m != ':') {
        printf("Wrong macro line format\r\n");
        return true;
    }
    m++;

    // FIXME: injecting the bus syntax into cmd line (i.e. simulating
    // user input) is probably not the best solution... :-/
    //printf("Inject cmd\r\n");
    // TODO: there is a way to advance through the cmdln queue that leaves a history pointer so up and down work
    // I think instead of of reading to end, we need to advance to the next buffer position
    char c;
    while (cmdln_try_remove(&c)) ;
    while (*m && cmdln_try_add(m))
        m++;
    cmdln_try_add('\0');
    //printf("Process syntax\r\n");
    return ui_process_syntax(); //I think we're going to run into issues with the ui_process loop if &&||; are used....
}

void disk_show_macro_file(const char *location)
{
    FIL fil;
    FRESULT fr;
    char line[512] = "\0";

    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    while (f_gets(line, sizeof(line), &fil)) {
        uint8_t line_len = strlen(line);
        // Remove trailing '\n' if present
        if (line_len>1 && line[line_len-1]=='\n')
            line[line_len-1] = '\0';
        // Show only usage and macros
        if (line[0]=='#' && line[1]=='!' && line[2])
            printf("%s.-%s %s%s\r\n", ui_term_color_prompt(), ui_term_color_info(), line+3, ui_term_color_reset()); // TODO: pretty print comment in VT100
        else if ((uint8_t)strtol(line, NULL, 10) > 0)
            printf("%s%s%s\r\n\r\n",ui_term_color_prompt(), line, ui_term_color_reset()); // TODO: pretty print macro in VT100
    }
    f_close(&fil);
}

void disk_get_line_id(const char *location, uint8_t id, char *line, int max_len)
{
    FIL fil;
    FRESULT fr;

    *line = '\0';

    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        return;
    }
    while (f_gets(line, max_len, &fil)) {
        if ((uint8_t)strtol(line, NULL, 10) == id) {
            uint8_t line_len = strlen(line);
            if (line_len>1 && line[line_len-1]=='\n')
                line[line_len-1] = '\0';
            break;
        }
    }
    f_close(&fil);
}

