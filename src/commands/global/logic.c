#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "pirate/button.h"
#include "binmode/fala.h"
#include "toolbars/logic_bar.h"
#include "binmode/logicanalyzer.h"
#include "lib/bp_args/bp_cmd.h"

enum logic_actions {
    LOGIC_START = 1,
    LOGIC_STOP,
    LOGIC_HIDE,
    LOGIC_SHOW,
    LOGIC_NAV
};

static const bp_command_action_t logic_action_defs[] = {
    { LOGIC_START, "start", T_HELP_LOGIC_START },
    { LOGIC_STOP,  "stop",  T_HELP_LOGIC_STOP },
    { LOGIC_HIDE,  "hide",  T_HELP_LOGIC_HIDE },
    { LOGIC_SHOW,  "show",  T_HELP_LOGIC_SHOW },
    { LOGIC_NAV,   "nav",   T_HELP_LOGIC_NAV },
};

static const bp_command_opt_t logic_opts[] = {
    { "info",       'i', BP_ARG_NONE,     NULL,       T_HELP_LOGIC_INFO },
    { "oversample", 'o', BP_ARG_REQUIRED, "<rate>",   T_HELP_LOGIC_OVERSAMPLE },
    { "frequency",  'f', BP_ARG_REQUIRED, "<freq>",   T_HELP_LOGIC_FREQUENCY },
    { "lowchar",    '0', BP_ARG_REQUIRED, "<char>",   T_HELP_LOGIC_LOW_CHAR },
    { "highchar",   '1', BP_ARG_REQUIRED, "<char>",   T_HELP_LOGIC_HIGH_CHAR },
    { "debug",      'd', BP_ARG_REQUIRED, "<level>",  T_HELP_LOGIC_DEBUG },
    { "base",       'b', BP_ARG_REQUIRED, "<pin>",    T_HELP_LOGIC_INFO },  // undocumented
    { 0 }
};

static const char* const usage[] = {
    "logic analyzer usage",
    "logic\t[start|stop|hide|show|nav]",
    "\t[-i] [-g] [-o oversample] [-f frequency] [-d debug]",
    "start logic analyzer:%s logic start",
    "stop logic analyzer:%s logic stop",
    "hide logic analyzer:%s logic hide",
    "show logic analyzer:%s logic show",
    "navigate logic analyzer:%s logic nav",
    "configure logic analyzer:%s logic -i -o 8 -f 1000000 -d 0",
    #if (BP_VER == 5 || BP_VER == XL5)
        "set base pin (0=bufdir, 8=bufio):%s -b: logic -b 8",
    #elif (BP_VER == 6 || BP_VER == 7)
        "set base pin (0=bufdir, 8=bufio, 20=follow along):%s -b: logic -b 20",
    #else
        #error "Unknown Bus Pirate version in logic.c"
    #endif
};

const bp_command_def_t logic_def = {
    .name         = "logic",
    .description  = T_HELP_LOGIC,
    .actions      = logic_action_defs,
    .action_count = count_of(logic_action_defs),
    .opts         = logic_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void logic_handler(struct command_result* res) {
    static bool logic_active = false;
    static bool logic_visible = false;

    if (bp_cmd_help_check(&logic_def, res->help_flag)) {
        return;
    }

    // Check for action verb
    uint32_t logic_action = 0;
    bool has_action = bp_cmd_get_action(&logic_def, &logic_action);

    if (has_action) {
        switch (logic_action) {
            case LOGIC_NAV:
                if (!logic_active) {
                    printf("Logic analyzer not active\r\n");
                    return;
                }
                logic_bar_navigate();
                return;

            case LOGIC_START:
                if (logic_active) {
                    printf("Logic analyzer already active\r\n");
                    return;
                }
                if (!logic_bar_start()) {
                    printf("Logic analyzer failed to start\r\n");
                    return;
                }
                logic_active = true;
                logic_visible = true;
                return;

            case LOGIC_STOP:
                if (!logic_active) {
                    printf("Logic analyzer not active\r\n");
                    return;
                }
                logic_active = false;
                logic_visible = false;
                logic_bar_stop();
                return;

            case LOGIC_HIDE:
                if (!logic_active) {
                    printf("Logic analyzer not active\r\n");
                    return;
                }
                if (!logic_visible) {
                    printf("Logic analyzer already hidden\r\n");
                    return;
                }
                logic_visible = false;
                logic_bar_hide();
                return;

            case LOGIC_SHOW:
                if (!logic_active) {
                    printf("Logic analyzer not active\r\n");
                    return;
                }
                if (logic_visible) {
                    printf("Logic analyzer already visible\r\n");
                    return;
                }
                logic_visible = true;
                logic_bar_show();
                logic_bar_update();
                return;
        }
    }

    // todo: config object for LA (copy sump???)
    // pass config to logicanalyzer.c
    bool has_info = bp_cmd_find_flag(&logic_def, 'i'); // info: show current settings
    uint32_t oversample;
    bool has_oversample = bp_cmd_get_uint32(&logic_def, 'o', &oversample); // oversample: set oversample rate
    uint32_t frequency;
    bool has_frequency = bp_cmd_get_uint32(&logic_def, 'f', &frequency); // frequency: set sample rate
    uint32_t debug_level;
    bool has_debug = bp_cmd_get_uint32(&logic_def, 'd', &debug_level); // debug: set debug level
    char low_char[3];
    bool has_low_char = bp_cmd_get_string(&logic_def, '0', low_char, sizeof(low_char)); // low: set low char
    char high_char[3];
    bool has_high_char = bp_cmd_get_string(&logic_def, '1', high_char, sizeof(high_char)); // high: set high char
    uint32_t base_channel;
    bool has_base_channel = bp_cmd_get_uint32(&logic_def, 'b', &base_channel); // base channel: set base channel

    bool has_ok=false;

    if (has_base_channel) {
        printf("Base channel set to: %d\r\n", base_channel);
        logic_analyzer_set_base_pin(base_channel);
        has_ok = true;
    }

    if (has_low_char) {
        printf("Low char set to: %c\r\n", low_char[0]);
        logic_bar_config(low_char[0], 0x00);
        has_ok = true;
    }

    if (has_high_char) {
        printf("High char set to: %c\r\n", high_char[0]);
        logic_bar_config(0x00, high_char[0]);
        has_ok = true;
    }

    if (has_debug) {
        if (debug_level > 2) {
            printf("Error: debug level must be 0, 1, or 2, '%d' is invalid\r\n", debug_level);
            res->error = true;
            return;
        }
        printf("Debug level set to: %d\r\n", debug_level);
        // update fala config struct
        fala_config.debug_level = debug_level;
        has_ok = true;
    }

    if (has_oversample) {
        if (oversample < 1) {
            printf("Error: oversample rate must be greater than 0, '%d' is invalid\r\n", oversample);
            res->error = true;
            return;
        }
        printf("Oversample rate set to: %d\r\n", oversample);
        // update fala config struct
        fala_config.oversample = oversample;
        has_ok = true;
    }

    if (has_frequency) {
        printf("Sample frequency set to: %dHz\r\n", frequency);
        // update fala config struct
        fala_config.base_frequency = frequency;
        has_ok = true;
    }

    // show help if nothing else is specified
    if (!has_ok) {
        bp_cmd_help_show(&logic_def);
        return;
    }

    if (has_info || has_oversample || has_frequency) {
        fala_config.actual_sample_frequency =
            logic_analyzer_compute_actual_sample_frequency(fala_config.base_frequency * fala_config.oversample, NULL);
        printf("\r\nLogic Analyzer settings\r\n");
        float foversample = (float)fala_config.actual_sample_frequency / fala_config.base_frequency;
        printf(" Oversample rate: %d\r\n", fala_config.oversample);
        printf(" Sample frequency: %dHz\r\n", fala_config.base_frequency);
        if (foversample != 1.0) {
            printf("\r\nNote: actual oversample rate is not 1\r\n");
        }
        if (fala_config.actual_sample_frequency != fala_config.base_frequency) {
            printf("Actual sample frequency: %dHz (%f * %dHz)\r\n",
                   fala_config.actual_sample_frequency,
                   foversample,
                   fala_config.base_frequency);
        }
    }
}
