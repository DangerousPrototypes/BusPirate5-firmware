#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"
#include "opt_args.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
#include "ui/ui_term.h"     // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "commands/global/script.h"
#include "button_scr.h"

#define BUTTON_FLAG_HIDE_COMMENTS 1u << 0
#define BUTTON_FLAG_EXIT_ON_ERROR 1u << 1
#define BUTTON_FLAG_FILE_CONFIGURED 1u << 2

uint8_t button_flags[BP_BUTT_MAX - 1];
char button_script_files[BP_BUTT_MAX - 1][BP_FILENAME_MAX];

typedef struct {
    const char* verb;
    const char* default_file;
} button_press_type_t;

// array of button press types
static button_press_type_t button_press_types[] = { { "short", "button.scr" }, { "long", "buttlong.scr" } };

static const uint8_t num_button_press_types = count_of(button_press_types);

static const char* const usage[] = {
    "button [short|long] [-f <file>] [-d (hiDe comments)] [-e(xit on error)] [-h(elp)]",
    "Assign script file to short button press: button short -f example.scr",
    "Assign script file to long button press: button long -f example.scr",
    "Exit script on error option: button short example.scr -e",
    "Default script files are 'button.scr' and 'buttlong.scr' in the root directory",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_BUTTON },        { 0, "short", T_HELP_BUTTON_SHORT }, { 0, "long", T_HELP_BUTTON_LONG },
    { 0, "-f", T_HELP_BUTTON_FILE }, { 0, "-d", T_HELP_BUTTON_HIDE },     { 0, "-e", T_HELP_BUTTON_EXIT },
    { 0, "-h", T_HELP_FLAG },
};

void button_scr_handler(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    // find our action
    char action[6]; // short or long
    uint8_t button_code;

    // first thing following the command (0) is the action (1)
    // determine short or long etc
    // This could be an array of string references. Then strcmp in a loop and use the index as the action type
    cmdln_args_string_by_position(1, sizeof(action), action);

    for (uint8_t i = 0; i < num_button_press_types; i++) {
        if (strcmp(action, button_press_types[i].verb) == 0) {
            button_code = i + 1;
            break;
        }
    }

    if (button_code == 0) {
        printf("Invalid action. Try button -h for help\r\n");
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        res->error = true;
        return;
    }

    // grab the file name, error if none
    char button_script_file[BP_FILENAME_MAX];
    command_var_t arg;
    if (!cmdln_args_find_flag_string('f', &arg, BP_FILENAME_MAX - 1, button_script_file)) {
        printf("Specify a script file with the -f flag (-f script.scr)\r\n");
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;
    }

    // reset all button flags
    button_flags[button_code - 1] = 0;

    // check if file exists...this should probably be a reusable function in storage.c
    FIL fil;    /* File object needed for each open file */
    FRESULT fr; /* FatFs return code */
    fr = f_open(&fil, button_script_file, FA_READ);
    if (fr != FR_OK) {
        printf("'%s' not found\r\n", button_script_file);
        res->error = true;
        return;
    }
    f_close(&fil);
    button_flags[button_code - 1] |= BUTTON_FLAG_FILE_CONFIGURED;

    // copy to button press type array
    memcpy(button_script_files[button_code - 1], button_script_file, BP_FILENAME_MAX - 1);
    printf("Button script file set to '%s'\r\n", button_script_file);

    // set other flag options
    if (cmdln_args_find_flag('d' | 0x20)) {
        button_flags[button_code - 1] |= BUTTON_FLAG_HIDE_COMMENTS;
    }
    if (cmdln_args_find_flag('e' | 0x20)) {
        button_flags[button_code - 1] |= BUTTON_FLAG_EXIT_ON_ERROR;
    }
}

bool button_exec(enum button_codes button_code) {
    const char* script_file = button_script_files[button_code - 1];

    if (!(button_flags[button_code - 1] & BUTTON_FLAG_FILE_CONFIGURED)) {
        // how long is the string
        uint8_t file_name_len = strlen(button_press_types[button_code - 1].default_file);
        // copy the default to the button script file
        memcpy(button_script_files[button_code - 1], button_press_types[button_code - 1].default_file, file_name_len);
        button_flags[button_code - 1] |= BUTTON_FLAG_FILE_CONFIGURED;
        printf("Using default '%s'\r\n", button_script_files[button_code - 1]);
    }
    printf("\r\n");

    if (script_exec(button_script_files[button_code - 1],
                    false,
                    !(button_flags[button_code - 1] & BUTTON_FLAG_HIDE_COMMENTS),
                    false,
                    (button_flags[button_code - 1] & BUTTON_FLAG_EXIT_ON_ERROR))) {
        printf("\r\nError in script file '%s'. Try button -h for help\r\n", script_file);
        return true;
    }
    return false;
}
