#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "ui/ui_cmdln.h"
#include "binmode/binmodes.h"
#include "binmode/fala.h"
#include "lib/bp_args/bp_cmd.h"

/*
 * =============================================================================
 * Action delegate — queries modes[] directly, no parallel array
 * =============================================================================
 */

/**
 * @brief Return the protocol_name at index, or NULL if past end.
 */
static const char *mode_verb_at(uint32_t index) {
    return (index < MAXPROTO) ? modes[index].protocol_name : NULL;
}

/**
 * @brief Case-insensitive match of a user token against modes[].protocol_name.
 * @details Writes the mode index into *action_out on match.
 */
static bool mode_match(const char *tok, size_t len, uint32_t *action_out) {
    for (uint32_t i = 0; i < MAXPROTO; i++) {
        size_t nlen = strlen(modes[i].protocol_name);
        if (nlen != len) continue;
        bool match = true;
        for (size_t j = 0; j < len; j++) {
            if ((tok[j] | 0x20) != (modes[i].protocol_name[j] | 0x20)) {
                match = false;
                break;
            }
        }
        if (match) {
            *action_out = i;
            return true;
        }
    }
    return false;
}

/**
 * @brief Return the setup_def for a resolved mode action, or NULL if none.
 * @details Used by linenoise hint/completion to switch to the mode's
 *          flag definitions after the verb is resolved.
 */
static const bp_command_def_t *mode_def_for_verb(uint32_t action) {
    return (action < MAXPROTO) ? (const bp_command_def_t *)modes[action].setup_def : NULL;
}

static const bp_action_delegate_t mode_delegate = {
    .verb_at      = mode_verb_at,
    .match        = mode_match,
    .def_for_verb = mode_def_for_verb,
};

/*
 * =============================================================================
 * Command definition
 * =============================================================================
 */

static const char * const mode_usage[] = {
    "m [-h] [mode] [-h] [mode_flags]",
    "Change mode with menu:%s m",
    "Change mode to UART:%s m uart",
    "Change mode to UART with baud 9600:%s m uart -b 9600",
    "m command help:%s m -h",
    "m uart mode help:%s m uart -h",
};

const bp_command_def_t mode_def = {
    .name        = "m",
    .description = T_CMDLN_MODE,
    .action_delegate = &mode_delegate,
    .usage       = mode_usage,
    .usage_count = 3,
};

/*
 * =============================================================================
 * Interactive mode selection prompt
 * =============================================================================
 */

/**
 * @brief Print mode list and prompt user to pick one.
 * @param[out] mode  Selected mode index (0-based)
 * @return true if user picked a mode, false if exited
 */
static bool mode_interactive_prompt(uint32_t *mode) {
    printf("\r\n%s%s%s\r\n", ui_term_color_info(),
           GET_T(T_MODE_MODE_SELECTION), ui_term_color_reset());

    for (uint32_t i = 0; i < MAXPROTO; i++) {
        printf(" %lu. %s%s%s\r\n",
               (unsigned long)(i + 1),
               ui_term_color_info(),
               modes[i].protocol_name,
               ui_term_color_reset());
    }

    printf("\r\n%sx to exit ", ui_term_color_prompt());
    printf(">%s \x03", ui_term_color_reset());

    if (!ui_prompt_user_input()) return false;

    prompt_result result;
    uint32_t pick;
    ui_parse_get_uint32(&result, &pick);

    if (result.exit) return false;
    if (!result.success || pick == 0 || pick > MAXPROTO) {
        ui_prompt_invalid_option();
        return false;
    }

    *mode = pick - 1;
    return true;
}

/*
 * =============================================================================
 * Mode change command handler
 * =============================================================================
 */

void ui_mode_enable_args(struct command_result* res) {
    // Contextual help: "m uart -h" shows UART flags, "m -h" shows mode list
    if (res->help_flag) {
        uint32_t help_mode;
        if (bp_cmd_get_action(&mode_def, &help_mode) &&
            modes[help_mode].setup_def) {
            bp_cmd_help_show((const bp_command_def_t *)modes[help_mode].setup_def);
        } else {
            bp_cmd_help_show(&mode_def);
        }
        return;
    }

    uint32_t mode;

    // Try to get mode name from command line via delegate
    if (!bp_cmd_get_action(&mode_def, &mode)) {
        // No mode on command line — interactive prompt
        if (!mode_interactive_prompt(&mode)) return;
    }

    printf("\r\n%s%s:%s %s",
        ui_term_color_info(),
        GET_T(T_MODE_MODE),
        ui_term_color_reset(),
        modes[mode].protocol_name);

    // Run mode setup dialog (may prompt for params)
    if (!modes[mode].protocol_setup()) return;

    modes[system_config.mode].protocol_cleanup();   // switch to HiZ
    modes[HIZ].protocol_setup_exc();                  // disables power supply etc.
    system_config.mode = mode;                      // setup the new mode
    if (!modes[system_config.mode].protocol_setup_exc()) {
        printf("\r\nFailed to setup mode %s", modes[system_config.mode].protocol_name);
        modes[HIZ].protocol_setup_exc();
        system_config.mode = 0;
    }
    fala_mode_change_hook();                        // notify follow along logic analyzer
}

bool int_display_menu(const struct ui_prompt* menu) {
    printf(" %sCurrent setting: %s%s\r\n",
           ui_term_color_info(),
           ui_const_display_formats[system_config.display_format],
           ui_term_color_reset());
    for (uint i = 0; i < (*menu).menu_items_count; i++) {
        printf(" %d. %s%s%s\r\n", i + 1, ui_term_color_info(), ui_const_display_formats[i], ui_term_color_reset());
    }
}

// set display mode  (hex, bin, octa, dec)
void ui_mode_int_display_format(struct command_result* res) {
    uint32_t mode;
    bool error;

    prompt_result result;
    ui_parse_get_attributes(&result, &mode, 1);

    if (result.error || result.no_value || result.exit || ((mode) > count_of(ui_const_display_formats)) ||
        ((mode) == 0)) {
        if (result.success && (mode) > count_of(ui_const_display_formats)) {
            ui_prompt_invalid_option();
        }
        error = 1;
    } else {
        (mode)--; // adjust down one from user choice
        error = 0;
    }

    if (error) // no integer found
    {
        static const struct ui_prompt_config cfg = {
            true,                            // bool allow_prompt_text;
            false,                           // bool allow_prompt_defval;
            false,                           // bool allow_defval;
            true,                            // bool allow_exit;
            &int_display_menu,               // bool (*menu_print)(const struct ui_prompt* menu);
            &ui_prompt_prompt_ordered_list,  // bool (*menu_prompt)(const struct ui_prompt* menu);
            &ui_prompt_validate_ordered_list // bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
        };

        static const struct ui_prompt mode_menu = {
            T_MODE_NUMBER_DISPLAY_FORMAT, 0, count_of(ui_const_display_formats), T_MODE_MODE, 0, 0, 0, 0, &cfg
        };

        prompt_result result;
        ui_prompt_uint32(&result, &mode_menu, &mode);
        if (result.exit) // user bailed
        {
            (*res).error = true;
            return;
        }
        mode--;
    }

    system_config.display_format = (uint8_t)mode;

    printf("\r\n%s%s:%s %s",
           ui_term_color_info(),
           GET_T(T_MODE_MODE),
           ui_term_color_reset(),
           ui_const_display_formats[system_config.display_format]);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
