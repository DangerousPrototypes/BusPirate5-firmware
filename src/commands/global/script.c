#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "lib/bp_args/bp_cmd.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "ui/ui_term.h"    // Terminal functions
#include "ui/ui_term_linenoise.h"
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "commands/global/script.h"

static const char* const usage[] = {
    "script <file> [-p(ause for <enter>)] [-d (hiDe comments)] [-a(bort on error)] [-h(elp)]",
    "Run script:%s script example.scr",
    "",
    "Script files:",
    "Script files are stored in text files with the .scr extension",
    "Lines starting with '#' are comments",
    "Other lines are inserted into the command prompt",
    "Exit with 'x' during execution",
    "Example:",
    "%s# This is my example script file",
    "%s# The 'pause' command waits for any key press",
    "%spause",
    "%s# Did it pause?",
};

static const bp_command_opt_t script_opts[] = {
    { "pause",      'p', BP_ARG_NONE, NULL, T_HELP_CMD_SCRIPT_PAUSE },
    { "hide",'d', BP_ARG_NONE, NULL, T_HELP_CMD_SCRIPT_HIDE_COMMENTS },
    { "abort", 'a', BP_ARG_NONE, NULL, T_HELP_CMD_SCRIPT_ERROR_EXIT },
    { 0 }
};

const bp_command_def_t script_def = {
    .name         = "script",
    .description  = T_HELP_CMD_SCRIPT,
    .actions      = NULL,
    .action_count = 0,
    .opts         = script_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void script_handler(struct command_result* res) {
    // check help
    if (bp_cmd_help_check(&script_def, res->help_flag)) {
        return;
    }

    bool pause_for_input = bp_cmd_find_flag(&script_def, 'p');
    bool show_comments = !bp_cmd_find_flag(&script_def, 'd');
    bool exit_on_error = bp_cmd_find_flag(&script_def, 'a');
    char location[32];
    if(!bp_cmd_get_positional_string(&script_def, 1, location, sizeof(location))){
        printf("Specify a script file to run\r\n");
        bp_cmd_help_show(&script_def);
        res->error = true;
        return;
    }
    if (script_exec(location, pause_for_input, show_comments, false, exit_on_error)) {
        res->error = true;
    }
}

// non zero return value indicates error
bool script_exec(char* location, bool pause_for_input, bool show_comments, bool show_tip, bool exit_on_error) {

    FIL fil;    /* File object needed for each open file */
    FRESULT fr; /* FatFs return code */
    char file[512];
    fr = f_open(&fil, location, FA_READ);
    if (fr != FR_OK) {
        storage_file_error(fr);
        return true;
    }
    /* Read every line and display it */
    // char c;
    // while (cmdln_try_remove(&c));
    while (f_gets(file, sizeof(file), &fil)) {
        if (file[0] == '#') { // comment line TODO: make a little more robust against leading whitespace
            if (show_comments) {
                printf("%s%s%s\r", ui_term_color_info(), file, ui_term_color_reset());
            }
        } else {
            if (show_tip) {
                printf("%sTip: <enter> to continue%s\r\n", ui_term_color_prompt(), ui_term_color_reset());
                show_tip = false;
            }
            if (system_config.subprotocol_name) {
                printf("%s%s-(%s)>%s ",
                       ui_term_color_prompt(),
                       modes[system_config.mode].protocol_name,
                       system_config.subprotocol_name,
                       ui_term_color_reset());
            } else {
                printf(
                    "%s%s>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, ui_term_color_reset());
            }
            // Strip trailing \r\n and inject into linenoise buffer
            {
                size_t slen = 0;
                for (size_t i = 0; i < sizeof(file); i++) {
                    if (file[i] == '\r' || file[i] == '\n' || file[i] == '\0') {
                        break;
                    }
                    slen++;
                }
                file[slen] = '\0';
            }
            ui_term_linenoise_inject_string(file);

            if (pause_for_input) {
                char c;
                while(!rx_fifo_try_get(&c)); // user hit enter
                if(c|0x20 == 'x') {
                    printf("Script execution aborted by user\r\n");
                    f_close(&fil);
                    return false;
                }
            }

            printf("\r\n");
            bool error = ui_process_commands();
            if (error && exit_on_error) {
                return true;
            }
            char c;
           if(rx_fifo_try_get(&c)){
                if(c|0x20 == 'x') {
                    printf("Script execution aborted by user\r\n");
                    f_close(&fil);
                    return false;
                }            
            }
        }
    }
    printf("\r\n");
    /* Close the file */
    f_close(&fil);
    return false;
}