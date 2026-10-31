#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_term.h"     // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_help.h" // Functions to display help in a standardized way
#include "commands/global/script.h"
#include "button_scr.h"
#include "lib/bp_args/bp_cmd.h"

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

enum button_scr_actions {
    BUTTON_SHORT = 1,
    BUTTON_LONG = 2
};

static const bp_command_action_t button_scr_action_defs[] = {
    { BUTTON_SHORT, "short", T_HELP_BUTTON_SHORT },
    { BUTTON_LONG,  "long",  T_HELP_BUTTON_LONG },
};

static const bp_command_opt_t button_scr_opts[] = {
    { "file",    'f', BP_ARG_REQUIRED, "file", T_HELP_BUTTON_FILE },
    { "hide",    'd', BP_ARG_NONE,     NULL,     T_HELP_BUTTON_HIDE },
    { "exit",    'e', BP_ARG_NONE,     NULL,     T_HELP_BUTTON_EXIT },
    { 0 }
};

static const char* const usage[] = {
    "button [short|long] [-f <file>] [-d (hiDe comments)] [-e(xit on error)] [-h(elp)]",
    "Assign script file to short button press:%s button short -f example.scr",
    "Assign script file to long button press:%s button long -f example.scr",
    "Exit script on error option:%s button short example.scr -e",
    "Default script files are 'button.scr' and 'buttlong.scr' in the root directory",
};

const bp_command_def_t button_scr_def = {
    .name         = "button",
    .description  = T_HELP_BUTTON,
    .actions      = button_scr_action_defs,
    .action_count = count_of(button_scr_action_defs),
    .opts         = button_scr_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void button_scr_handler(struct command_result* res) {
    // check help
    if (bp_cmd_help_check(&button_scr_def, res->help_flag)) {
        return;
    }

    // get action
    uint32_t button_action = 0;
    if (!bp_cmd_get_action(&button_scr_def, &button_action)) {
        bp_cmd_help_show(&button_scr_def);
        res->error = true;
        return;
    }

    uint8_t button_code = (uint8_t)button_action;  // BUTTON_SHORT=1 or BUTTON_LONG=2

    // grab the file name, error if none
    char button_script_file[BP_FILENAME_MAX];
    if (!bp_cmd_get_string(&button_scr_def, 'f', button_script_file, BP_FILENAME_MAX - 1)) {
        printf("Specify a script file with the -f flag (-f script.scr)\r\n");
        bp_cmd_help_show(&button_scr_def);
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
    if (bp_cmd_find_flag(&button_scr_def, 'd')) {
        button_flags[button_code - 1] |= BUTTON_FLAG_HIDE_COMMENTS;
    }
    if (bp_cmd_find_flag(&button_scr_def, 'e')) {
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
