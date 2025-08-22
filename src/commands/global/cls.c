#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_statusbar.h"
#include "ui/ui_help.h"
#include "ui/ui_flags.h"

static const char* const usage[] = {
    "cls",
    "Clear and refresh the terminal screen:%s cls",
    "Note: will attempt to detect and initialize VT100 ANSI terminal",
};

static const struct ui_help_options options[] = { 0 };

void ui_display_clear(struct command_result* res) {
    BP_ASSERT_CORE0();
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    if (!system_config.terminal_ansi_color) {
        printf("cls command is only supported in VT100 terminal mode");
        return;
    }

    ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
    ui_term_init();   // Initialize VT100 if ANSI terminal
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        ui_statusbar_init();
        ui_statusbar_update_blocking();
    }
}
