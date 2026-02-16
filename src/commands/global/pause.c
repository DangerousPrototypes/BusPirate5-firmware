#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "lib/bp_args/bp_cmd.h"
#include "pirate/button.h"

static const char* const usage[] = {
    "pause and wait for any key:%s pause",
    "pause and wait for button press:%s pause -b",
    "pause and wait for button or any key:%s pause -b -k",
    "'x' key to exit (e.g. script mode):%s pause -x",
};

static const bp_command_opt_t pause_opts[] = {
    { "key",    'k', BP_ARG_NONE, NULL, T_HELP_CMD_PAUSE_KEY },
    { "button", 'b', BP_ARG_NONE, NULL, T_HELP_CMD_PAUSE_BUTTON },
    { "exit",   'x', BP_ARG_NONE, NULL, T_HELP_CMD_PAUSE_EXIT },
    { 0 }
};

const bp_command_def_t pause_def = {
    .name         = "pause",
    .description  = T_HELP_CMD_PAUSE,
    .actions      = NULL,
    .action_count = 0,
    .opts         = pause_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void pause_handler(struct command_result* res) {
    if (bp_cmd_help_check(&pause_def, res->help_flag)) {
        return;
    }

    // check for -b button flag
    bool pause_for_button = bp_cmd_find_flag(&pause_def, 'b');
    bool pause_for_key = bp_cmd_find_flag(&pause_def, 'k') || !pause_for_button;
    bool exit_on_x = bp_cmd_find_flag(&pause_def, 'x');

    // print option messages
    if (pause_for_key) {
        printf("%s\r\n", GET_T(T_PRESS_ANY_KEY));
    }
    if (pause_for_button) {
        printf("%s\r\n", GET_T(T_PRESS_BUTTON));
    }
    if (exit_on_x) {
        printf("%s\r\n", GET_T(T_PRESS_X_TO_EXIT));
    }

    for (;;) {

        // even if we are not pausing for key or button,
        // it is cleaner if we consume any errant characters to
        // avoid unexpected behavior after the pause
        char c;
        if (rx_fifo_try_get(&c)) {
            if (exit_on_x && ((c | 0x20) == 'x')) {
                res->error = true;
                printf("%s\r\n", GET_T(T_EXIT));
                return;
            }
            if (pause_for_key) {
                return;
            }
        }

        if (pause_for_button) {
            if (button_get(0)) {
                return;
            }
        }

        if (system_config.error) {
            res->error = true;
            return;
        }
    }
}
