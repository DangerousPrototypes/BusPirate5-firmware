#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "modes.h"
#include "system_config.h"
#include "ui/ui_cmdln.h"
#include "displays.h"
#include "lib/bp_args/bp_cmd.h"

// Syntax operators: not real commands, always manually maintained
static const struct ui_help_options syntax_help[] = {
    { 1, "", T_HELP_SECTION_SYNTAX },
    { 0, "[/{", T_HELP_2_3 },
    { 0, "]/}", T_HELP_2_4 },
    { 0, "123", T_HELP_2_8 },
    { 0, "0x123", T_HELP_2_9 },
    { 0, "0b110", T_HELP_2_10 },
    { 0, "\"abc\"", T_HELP_2_7 },
    { 0, "r", T_HELP_2_11 },
    { 0, "/", T_HELP_SYN_CLOCK_HIGH },
    { 0, "\\", T_HELP_SYN_CLOCK_LOW },
    { 0, "^", T_HELP_SYN_CLOCK_TICK },
    { 0, "-", T_HELP_SYN_DATA_HIGH },
    { 0, "_", T_HELP_SYN_DATA_LOW },
    { 0, ".", T_HELP_SYN_DATA_READ },
    { 0, ":", T_HELP_2_19 },
    { 0, ".", T_HELP_2_20 },
    { 0, "d/D", T_HELP_1_6 },
    { 0, "a/A/@.x", T_HELP_1_7 },
    { 0, "v.x", T_HELP_SYNTAX_ADC },
    { 0, ">", T_HELP_GREATER_THAN },
};

static const struct ui_help_options global_commands_more_help[] = {
    // Get more help
    { 1, "", T_HELP_SECTION_HELP },
    { 0, "?/help", T_HELP_HELP_GENERAL },
    { 0, "-h", T_HELP_HELP_COMMAND },
    { 1, "", T_HELP_HINT }
};

static const char* const help_usage[] = {
    "?|help [mode|display] [-h(elp)]",
    "Show help and all commands:%s help",
    "Show help and commands for current mode:%s help mode",
    "Show help and commands for current display mode:%s help display",
};

static const struct ui_help_options help_options[] = {
    { 1, "", T_HELP_HELP }, // command help
    { 0, "?/help", T_HELP_SYS_COMMAND },
    { 0, "mode", T_HELP_SYS_MODE },
    { 0, "display", T_HELP_SYS_DISPLAY },
    { 0, "-h", T_HELP_SYS_HELP },
};

const struct bp_command_def help_def = {
    .name = "help",
    .description = T_HELP_HELP,
    .usage = help_usage,
    .usage_count = count_of(help_usage)
};

void help_display(void) {
    if (displays[system_config.display].display_help) {
        displays[system_config.display].display_help();
    } else {
        printf("No display help available for this display mode\r\n");
    }
}

void help_mode(void) {
    // ui_help_options(&help_commands[0],count_of(help_commands));
    modes[system_config.mode].protocol_help();
}

void help_global(void) {
    // Auto-generated global commands grouped by category
    ui_help_global_commands();

    // Syntax reference (manual — not real commands)
    ui_help_options(&syntax_help[0], count_of(syntax_help));

    // Loop through modes and display available commands
    for (uint32_t i = 0; i < count_of(modes); i++) {
        if ((*modes[i].mode_commands_count) > 0 && modes[i].mode_commands->func != NULL) {
            ui_help_mode_commands_exec(modes[i].mode_commands, *modes[i].mode_commands_count, modes[i].protocol_name);
        }
    }

    // Show more help last
    ui_help_options(&global_commands_more_help[0], count_of(global_commands_more_help));
}

void help_handler(struct command_result* res) {
    // check help
    if (bp_cmd_help_check(&help_def, res->help_flag)) {
        return;
    }
    // check mode|global|display
    char action[9];
    bp_cmd_get_positional_string(&help_def, 1, action, sizeof(action));
    bool mode = (strcmp(action, "mode") == 0);
    bool display = (strcmp(action, "display") == 0);
    if (mode) {
        help_mode();
    } else if (display) {
        help_display();
    } else {
        help_global();
    }
}
