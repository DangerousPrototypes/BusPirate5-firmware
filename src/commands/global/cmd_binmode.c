#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "lib/bp_args/bp_cmd.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "msc_disk.h"
#include "binmode/binmodes.h"
#include "ui/ui_term.h"

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = {
    "binmode [number]",
    "Configure the active binary mode:%s binmode",
    "Select binmode 2:%s binmode 2",
};

static const bp_val_constraint_t binmode_range = {
    .type = BP_VAL_UINT32,
    .u = { .min = 1, .max = BINMODE_MAXPROTO, .def = 1 },
    .prompt = T_CONFIG_BINMODE_SELECT,
};

static const bp_command_positional_t binmode_positionals[] = {
    { "number", NULL, T_CONFIG_BINMODE_SELECT, false, &binmode_range },
    { 0 }
};

const bp_command_def_t cmd_binmode_def = {
    .name         = "binmode",
    .description  = T_CONFIG_BINMODE_SELECT,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals      = binmode_positionals,
    .positional_count = 1,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void cmd_binmode_handler(struct command_result* res) {
    // we can use the ui_help_show function to display the help text we configured above
    if (bp_cmd_help_check(&cmd_binmode_def, res->help_flag)) {
        // Current binmode 
        printf("%sActive binmode:%s %s\r\n", ui_term_color_info(), ui_term_color_reset(), binmodes[system_config.binmode_select].binmode_name);
        return;
    }

    uint32_t binmode_number;
    bp_cmd_status_t s = bp_cmd_positional(&cmd_binmode_def, 1, &binmode_number);
    if (s == BP_CMD_INVALID) {
        res->error = true;
        return;
    }
    if (s == BP_CMD_MISSING) {
        // print the dynamic binmode menu, then prompt for a number
        for (uint8_t i = 0; i < count_of(binmodes); i++) {
            printf(" %d. %s\r\n", i + 1, binmodes[i].binmode_name);
        }
        if (bp_cmd_prompt(&binmode_range, &binmode_number) != BP_CMD_OK) {
            res->error = true;
            return;
        }
    }

    binmode_number--;
    binmode_cleanup();
    printf("\r\n%sBinmode selected:%s %s\r\n",
           ui_term_color_info(),
           ui_term_color_reset(),
           binmodes[binmode_number].binmode_name);
    system_config.binmode_select = binmode_number;

    // optional: save system config options
    // NOTE: to be a saved mode, the mode MUST NOT output anything to the terminal
    // only modes that have been verified can be saved
    // outputting text before the terminal is open will cause crash on startup
    if(binmodes[system_config.binmode_select].can_save_config) {
        if (bp_cmd_confirm(NULL, "Save setting?")) {
            binmode_load_save_config(true); //save config
        }
    }

    //defer setup
    binmode_setup();

    if (binmodes[system_config.binmode_select].binmode_setup_message) {
        printf("\r\n");
        binmodes[system_config.binmode_select].binmode_setup_message();
    }
    
}


