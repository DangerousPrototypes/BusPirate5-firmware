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
/*
struct ui_help_topics {
    uint8_t topic_id;
    const char* description;
};

enum {
    HELP_TOPIC_IO = 0,
    HELP_TOPIC_CONFIGURE,
    HELP_TOPIC_SYSTEM,
    HELP_TOPIC_FILES,
    HELP_TOPIC_SCRIPT,
    HELP_TOPIC_TOOLS,
    HELP_TOPIC_MODE,
    HELP_TOPIC_SYNTAX,
};

const struct ui_help_topics help_topics[] = {
    { HELP_TOPIC_IO, T_HELP_SECTION_IO},
    { HELP_TOPIC_CONFIGURE, T_HELP_SECTION_CONFIGURE},
    { HELP_TOPIC_SYSTEM, T_HELP_SECTION_SYSTEM},
    { HELP_TOPIC_FILES, T_HELP_SECTION_FILES},
    { HELP_TOPIC_SCRIPT, T_HELP_SECTION_SCRIPT},
    { HELP_TOPIC_TOOLS, T_HELP_SECTION_TOOLS},
    { HELP_TOPIC_MODE, T_HELP_SECTION_MODE},
    { HELP_TOPIC_SYNTAX, T_HELP_SECTION_SYNTAX},
};
*/
const struct ui_help_options global_commands[] = {
    // BUGBUG -- Why isn't this automatically generated from _global_command_struct?
    //           Likely because this list is categorized.
    //           Unfortunately, that also means it's easy to get these out of sync.
    //           That problem will disappear when we restructure the commands to
    //           be heirarchical / well structures (needed if ever to enable protobuf).

    { 1, "", T_HELP_SECTION_IO }, // work with pins, input, output measurement
    { 0, "w/W", T_HELP_1_21 },    // note that pin functions need power on the buffer
    { 0, "a/A/@ x", T_HELP_COMMAND_AUX },
    { 0, "f x/F x", T_HELP_1_11 },
    { 0, "f/F", T_HELP_1_23 },
    { 0, "g x/G", T_HELP_1_12 },
    { 0, "p/P", T_HELP_1_18 },
    { 0, "v x/V x", T_HELP_1_22 },
    { 0, "v/V", T_HELP_1_10 },

    // measure analog and digital signals
    //{1,"",T_HELP_SECTION_CAPTURE},
    //     {0,"scope",T_HELP_CAPTURE_SCOPE},
    //     {0,"logic",T_HELP_CAPTURE_LA},

    // configure the terminal, LEDs, display and mode
    { 1, "", T_HELP_SECTION_CONFIGURE },
    { 0, "c", T_HELP_1_9 },
    { 0, "d", T_HELP_COMMAND_DISPLAY },
    { 0, "o", T_HELP_1_17 },
    { 0, "l/L", T_HELP_1_15 },
    { 0, "cls", T_HELP_CMD_CLS },

    // restart, firmware updates and diagnostic
    { 1, "", T_HELP_SECTION_SYSTEM },
    { 0, "i", T_HELP_1_14 },
    { 0, "reboot", T_HELP_SYSTEM_REBOOT },
    { 0, "$", T_HELP_1_5 },
    { 0, "~", T_HELP_1_3 },

    // list files and navigate the storage
    { 1, "", T_HELP_SECTION_FILES },
    { 0, "ls", T_HELP_CMD_LS },
    { 0, "cd", T_HELP_CMD_CD },
    { 0, "mkdir", T_HELP_CMD_MKDIR },
    { 0, "rm", T_HELP_CMD_RM },
    { 0, "cat", T_HELP_CMD_CAT },
    { 0, "hex", T_HELP_CMD_HEX }, //     Print HEX file contents
    { 0, "format", T_HELP_CMD_FORMAT },
    { 0, "label", T_HELP_CMD_LABEL },
    { 0, "image", T_HELP_CMD_IMAGE },
    { 0, "dump", T_HELP_CMD_DUMP },

    { 1, "", T_HELP_SECTION_SCRIPT },
    { 0, "script", T_HELP_CMD_SCRIPT },
    { 0, "tutorial", T_HELP_CMD_TUTORIAL },
    { 0, "button", T_HELP_CMD_BUTTON },
    { 0, "macro", T_HELP_CMD_MACRO },
    { 0, "(x)/(0)", T_HELP_2_1 },
    { 0, "pause", T_HELP_CMD_PAUSE },

    // tools and utilities
    { 1, "", T_HELP_SECTION_TOOLS },
    { 0, "logic", T_HELP_CMD_LOGIC },
    { 0, "smps", T_HELP_CMD_SMPS },
    { 0, "= x/| x", T_HELP_1_2 },

    // enter a mode to use protocols
    { 1, "", T_HELP_SECTION_MODE },
    { 0, "m", T_HELP_1_16 },
    { 0, "binmode", T_CONFIG_BINMODE_SELECT },

    // send and receive data in modes using bus syntax
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

const struct ui_help_options global_commands_more_help[] = {
    // Get more help
    { 1, "", T_HELP_SECTION_HELP },
    { 0, "?/help", T_HELP_HELP_GENERAL },
    //{0,"hd",    T_HELP_HELP_DISPLAY},
    { 0, "-h", T_HELP_HELP_COMMAND },

    { 1, "", T_HELP_HINT }
};

static const char* const help_usage[] = {
    "?|help [mode|display] [-h(elp)]",
    "Show global commands: ?",
    "Show help and commands for current mode: ? mode",
    "Show help and commands for current display mode: ? display",
};

static const struct ui_help_options help_options[] = {
    { 1, "", T_HELP_HELP }, // command help
    { 0, "?/help", T_HELP_SYS_COMMAND },
    { 0, "mode", T_HELP_SYS_MODE },
    { 0, "display", T_HELP_SYS_DISPLAY },
    { 0, "-h", T_HELP_SYS_HELP },
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
    // global commands help list
    ui_help_options(&global_commands[0], count_of(global_commands));

    // loop through modes and display available commands
    for (uint32_t i = 0; i < count_of(modes); i++) {
        if ((*modes[i].mode_commands_count) > 0) {
            // ui_help_mode_commands(modes[i].mode_commands, *modes[i].mode_commands_count);
            // printf("%d\r\n", *modes[i].mode_commands_count);
            ui_help_mode_commands_exec(modes[i].mode_commands, *modes[i].mode_commands_count, modes[i].protocol_name);
        }
    }
    // show more help last
    ui_help_options(&global_commands_more_help[0], count_of(global_commands_more_help));
}

void help_handler(struct command_result* res) {
    // check help
    if (ui_help_show(res->help_flag, help_usage, count_of(help_usage), &help_options[0], count_of(help_options))) {
        return;
    }
    // check mode|global|display
    char action[9];
    cmdln_args_string_by_position(1, sizeof(action), action);
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
