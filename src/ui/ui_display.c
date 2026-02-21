#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "displays.h"
#include "ui/ui_term.h"
#include "ui/ui_display.h"
#include "lib/bp_args/bp_cmd.h"

/*
 * =============================================================================
 * Display selection command ("d")
 * =============================================================================
 */

static const bp_val_choice_t display_choices[] = {
    { "default",  NULL, 0, DISP_DEFAULT  },
#ifdef BP_USE_SCOPE
    { "scope",    NULL, 0, DISP_SCOPE    },
#endif
    { "disabled", NULL, 0, DISP_DISABLED },
};

static const bp_val_constraint_t display_constraint = {
    .type = BP_VAL_CHOICE,
    .choice = {
        .choices = display_choices,
        .count   = count_of(display_choices),
        .def     = DISP_DEFAULT,
    },
    .prompt = T_MODE_DISPLAY_SELECTION,
};

static const bp_command_positional_t display_positionals[] = {
    { "display", NULL, T_MODE_DISPLAY_SELECTION, false, &display_constraint },
    { 0 },
};

static const char * const display_usage[] = {
    "d [-h] [display]",
    "Interactive menu:%s d",
    "Set default:%s d default",
    "Set by number:%s d 1",
};

const bp_command_def_t display_select_def = {
    .name        = "d",
    .description = T_CMDLN_DISPLAY,
    .positionals = display_positionals,
    .positional_count = 1,
    .usage       = display_usage,
    .usage_count = 4,
};

void ui_display_enable_args(struct command_result* res) {
    if (res->help_flag) {
        bp_cmd_help_show(&display_select_def);
        return;
    }

    uint32_t display;
    bp_cmd_status_t s = bp_cmd_positional(&display_select_def, 1, &display);

    if (s == BP_CMD_INVALID) {
        res->error = true;
        return;
    }

    if (s == BP_CMD_MISSING) {
        s = bp_cmd_prompt(&display_constraint, &display);
        if (s == BP_CMD_EXIT) {
            return;
        }
    }

    // ok, start setup dialog
    displays[system_config.display].display_cleanup(); // switch to HiZ
    if (!displays[display].display_setup())            // user bailed on setup steps
    {
        system_config.display = 0; // switch to default
        displays[system_config.display].display_setup();
        displays[system_config.display].display_setup_exc();
        printf("\r\n%s%s:%s %s",
               ui_term_color_info(),
               GET_T(T_MODE_DISPLAY),
               ui_term_color_reset(),
               displays[system_config.display].display_name);
        return;
    }

    system_config.display = display;                     // setup the new mode
    displays[system_config.display].display_setup_exc(); // execute the mode setup

    if (system_config.mode == 0) // TODO: do something to show the mode (LED? LCD?)
    {
        // gpio_clear(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    } else {
        // gpio_set(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    }

    printf("\r\n%s%s:%s %s",
           ui_term_color_info(),
           GET_T(T_MODE_DISPLAY),
           ui_term_color_reset(),
           displays[system_config.display].display_name);
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
