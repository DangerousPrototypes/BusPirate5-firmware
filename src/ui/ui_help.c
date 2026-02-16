#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "pirate/amux.h"
#include "ui/ui_term.h"
#include "command_struct.h" //remove?
#include "ui/ui_help.h"
#include "display/scope.h"
#include "system_config.h"
#include "bytecode.h"
#include "modes.h"
#include "pirate/bio.h"
#include "commands.h"
#include "lib/bp_args/bp_cmd.h"

/**
 * @brief Category heading translation keys.
 * @details Indexed by enum cmd_category. CMD_CAT_HIDDEN has no heading.
 */
static const uint32_t category_headings[] = {
    [CMD_CAT_IO]        = T_HELP_SECTION_IO,
    [CMD_CAT_CONFIGURE] = T_HELP_SECTION_CONFIGURE,
    [CMD_CAT_SYSTEM]    = T_HELP_SECTION_SYSTEM,
    [CMD_CAT_FILES]     = T_HELP_SECTION_FILES,
    [CMD_CAT_SCRIPT]    = T_HELP_SECTION_SCRIPT,
    [CMD_CAT_TOOLS]     = T_HELP_SECTION_TOOLS,
    [CMD_CAT_MODE]      = T_HELP_SECTION_MODE,
    [CMD_CAT_HIDDEN]    = 0, // never printed
};

void ui_help_global_commands(void) {
    for (uint8_t cat = 0; cat < CMD_CAT_HIDDEN; cat++) {
        // Print category heading
        if (category_headings[cat]) {
            printf("\r\n%s%s%s\r\n",
                   ui_term_color_info(),
                   GET_T(category_headings[cat]),
                   ui_term_color_reset());
        }

        // Walk commands[], print every entry matching this category
        for (uint32_t i = 0; i < commands_count; i++) {
            if (commands[i].category != cat) continue;

            // Resolve description: def->description > description_text > fallback
            const char *desc;
            if (commands[i].def && commands[i].def->description) {
                desc = GET_T(commands[i].def->description);
            } else if (commands[i].description_text) {
                desc = GET_T(commands[i].description_text);
            } else {
                desc = "No description. Try -h";
            }

            printf("%s%s%s\t%s%s%s\r\n",
                   ui_term_color_prompt(),
                   commands[i].command,
                   ui_term_color_reset(),
                   ui_term_color_info(),
                   desc,
                   ui_term_color_reset());
        }
    }
}

// displays the help
// NOTE: update if the count of "\r\n" prints in the switch statement below changes
// case1(3) + case0(3) + case\n(1) = 7
#define PAGER_PER_HELP_ROWS 7
void ui_help_options(const struct ui_help_options(*help), uint32_t count) {
    // set the pager rows for a pause, fix the integer wrap around if something weird is going on
    uint8_t pager_rows = system_config.terminal_ansi_rows - PAGER_PER_HELP_ROWS;
    if (pager_rows > system_config.terminal_ansi_rows) {
        pager_rows = system_config.terminal_ansi_rows;
    }

    for (uint i = 0; i < count; i++) {
        if ((i > 0) && ((i % pager_rows) == 0)) {
            ui_term_cmdln_wait_char('\0');
        }
        switch (help[i].help) {
            case 1: // heading
                printf("\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(help[i].description), ui_term_color_reset());
                break;
            case 0: // help item
                printf("%s%s%s\t%s%s%s\r\n",
                       ui_term_color_prompt(),
                       help[i].command,
                       ui_term_color_reset(),
                       ui_term_color_info(),
                       GET_T(help[i].description),
                       ui_term_color_reset());
                break;
            case '\n':
                printf("\r\n");
                break;
            default:
                break;
        }
    }
}

void ui_help_usage(const char* const flash_usage[], uint32_t count) {
    printf("usage:\r\n");
    for (uint32_t i = 0; i < count; i++) {
        printf("%s", ui_term_color_info());
        printf(flash_usage[i], ui_term_color_reset());
        printf("\r\n");
        //printf("%s%s%s\r\n", ui_term_color_info(), flash_usage[i], ui_term_color_reset());
    }
}

bool ui_help_show(bool help_flag,
                  const char* const usage[],
                  uint32_t count_of_usage,
                  const struct ui_help_options* options,
                  uint32_t count_of_options) {
    if (help_flag) {

        ui_help_usage(usage, count_of_usage);
        printf("%s", ui_term_color_reset());

        if(options[0].description!=0) {
            ui_help_options(&options[0], count_of_options);
        }
        return true;
    }
    return false;
}

void ui_help_mode_commands_exec(const struct _mode_command_struct* commands, uint32_t count, const char* mode) {
    // printf("\r\nAvailable mode commands:\r\n");
    printf(
        "\r\n%s%s%s mode commands:%s\r\n", ui_term_color_prompt(), mode, ui_term_color_info(), ui_term_color_reset());
    for (uint32_t i = 0; i < count; i++) {
        printf("%s%s%s\t%s%s\r\n",
               ui_term_color_prompt(),
               commands[i].command,
               ui_term_color_info(),
               commands[i].description_text ? GET_T(commands[i].description_text) : "Description not set. Try -h for command help",
               ui_term_color_reset());
    }
}

void ui_help_mode_commands(const struct _mode_command_struct* commands, uint32_t count) {
    ui_help_mode_commands_exec(commands, count, modes[system_config.mode].protocol_name);
}

// true if there is a voltage on out/vref pin
bool ui_help_check_vout_vref(void) {
    if (scope_running) { // scope is using the analog subsystem
        return true;     // can't check, just skip
    }

    amux_sweep();
    if (hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] <
        790) { // 0.8V minimum output allowed to set on internal PSU when reading i could be 0.79
        return false;
    }
    return true;
}

bool ui_help_sanity_check(bool vout, uint8_t pullup_mask) {
    bool ok=true;

    if (vout) {
        if (!ui_help_check_vout_vref()) {
            ui_help_error(T_MODE_NO_VOUT_VREF_ERROR);
            printf("%s%s%s\r\n", ui_term_color_info(), GET_T(T_MODE_NO_VOUT_VREF_HINT), ui_term_color_reset());
            ok=false;
        }
    }

    if (pullup_mask) {
        uint8_t temp; 
        for (uint8_t i = 0; i < 8; i++) {
            if (pullup_mask & (1 << i)) {
                temp |= (bio_get(i)<<i);
            }
        }
        //NOTE: to avoid too many false positives, 
        // we only check that at least one pin is pulled up
        // this is identical to the v3.x behavior
        if (!temp) {
            ui_help_error(T_MODE_NO_PULLUP_ERROR);
            printf("%s%s%s\r\n", ui_term_color_info(), GET_T(T_MODE_NO_PULLUP_HINT), ui_term_color_reset());
            ok=false;
        }
    }
    return ok;
}

// move to help?
void ui_help_error(uint32_t error) {
    printf("\x07\r\n%sError:%s %s\r\n", ui_term_color_error(), ui_term_color_reset(), GET_T(error));
}
