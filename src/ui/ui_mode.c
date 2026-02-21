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

/*
 * =============================================================================
 * Display format command ("o")
 * =============================================================================
 */

static const bp_val_choice_t display_format_choices[] = {
    { "auto",  NULL, 0, df_auto  },
    { "hex",   NULL, 0, df_hex   },
    { "dec",   NULL, 0, df_dec   },
    { "bin",   NULL, 0, df_bin   },
    { "ascii", NULL, 0, df_ascii },
};

static const bp_val_constraint_t display_format_constraint = {
    .type = BP_VAL_CHOICE,
    .choice = {
        .choices = display_format_choices,
        .count   = count_of(display_format_choices),
        .def     = df_auto,
    },
    .prompt = T_MODE_NUMBER_DISPLAY_FORMAT,
};

static const bp_command_positional_t display_format_positionals[] = {
    { "format", "auto/hex/dec/bin/ascii", T_MODE_NUMBER_DISPLAY_FORMAT, false, &display_format_constraint },
    { 0 },
};

static const char * const display_format_usage[] = {
    "o [-h] [format]",
    "Interactive menu:%s o",
    "Set hex:%s o hex",
    "Set decimal by number:%s o 3",
};

const bp_command_def_t display_format_def = {
    .name        = "o",
    .description = T_CMDLN_DISPLAY_FORMAT,
    .positionals = display_format_positionals,
    .positional_count = 1,
    .usage       = display_format_usage,
    .usage_count = 4,
};

// set display mode  (hex, bin, octa, dec)
void ui_mode_int_display_format(struct command_result* res) {
    if (res->help_flag) {
        bp_cmd_help_show(&display_format_def);
        return;
    }

    uint32_t fmt;
    bp_cmd_status_t s = bp_cmd_positional(&display_format_def, 1, &fmt);

    if (s == BP_CMD_INVALID) {
        res->error = true;
        return;
    }

    if (s == BP_CMD_MISSING) {
        // Show current setting before the interactive menu
        printf("\r\n %sCurrent setting: %s%s",
               ui_term_color_info(),
               ui_const_display_formats[system_config.display_format],
               ui_term_color_reset());

        s = bp_cmd_prompt(&display_format_constraint, &fmt);
        if (s == BP_CMD_EXIT) {
            res->error = true;
            return;
        }
    }

    system_config.display_format = (uint8_t)fmt;

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
