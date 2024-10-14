#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "ui/ui_cmdln.h"
#include "pirate/button.h"
#include "binmode/fala.h"
#include "toolbars/logic_bar.h"
#include "binmode/logicanalyzer.h"

static const char* const usage[] = {
    "logic analyzer usage",
    "logic\t[start|stop|hide|show|nav]",
    "\t[-i] [-g] [-o oversample] [-f frequency] [-d debug]",
    "start logic analyzer: logic start",
    "stop logic analyzer: logic stop",
    "hide logic analyzer: logic hide",
    "show logic analyzer: logic show",
    "navigate logic analyzer: logic nav",
    "configure logic analyzer: logic -i -o 8 -f 1000000 -d 0",
#if BP_VER == 6
    "undocumented: set base pin (0=bufdir, 8=bufio, 20=follow along) -b: logic -b 20",
#else
    "undocumented: set base pin (0=bufdir, 8=bufio) -b: logic -b 8",
#endif
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_LOGIC },            // flash command help
                                        // start, stop, hide, show, nav
    { 0, "start", T_HELP_LOGIC_START }, // start
    { 0, "stop", T_HELP_LOGIC_STOP },   // stop
    { 0, "hide", T_HELP_LOGIC_HIDE },   // hide
    { 0, "show", T_HELP_LOGIC_SHOW },   // show
    { 0, "nav", T_HELP_LOGIC_NAV },     // navigate
    // config options
    { 0, "-i", T_HELP_LOGIC_INFO },       // info
    { 0, "-o", T_HELP_LOGIC_OVERSAMPLE }, // oversample
    { 0, "-f", T_HELP_LOGIC_FREQUENCY },  // frequency
    { 0, "-0", T_HELP_LOGIC_LOW_CHAR },   // low char
    { 0, "-1", T_HELP_LOGIC_HIGH_CHAR },  // high char
    { 0, "-d", T_HELP_LOGIC_DEBUG },      // debug
    { 0, "-h", T_HELP_FLAG },
};

void logic_handler(struct command_result* res) {
    static bool logic_active = false;
    static bool logic_visible = false;

    if (ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options))) {
        return;
    }

    char action_str[9];
    bool verb_start = false, verb_stop = false, verb_hide = false, verb_show = false, verb_nav = false;
    if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)) {
        if (strcmp(action_str, "start") == 0) {
            verb_start = true;
        }
        if (strcmp(action_str, "stop") == 0) {
            verb_stop = true;
        }
        if (strcmp(action_str, "hide") == 0) {
            verb_hide = true;
        }
        if (strcmp(action_str, "show") == 0) {
            verb_show = true;
        }
        if (strcmp(action_str, "nav") == 0) {
            verb_nav = true;
        }
    }

    if (verb_nav) {
        if (!logic_active) {
            printf("Logic analyzer not active\r\n");
            return;
        }
        logic_bar_navigate();
        return;
    }

    if (verb_start) {
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
    }

    if (verb_stop) {
        if (!logic_active) {
            printf("Logic analyzer not active\r\n");
            return;
        }
        logic_active = false;
        logic_visible = false;
        logic_bar_stop();
        return;
    }

    if (verb_hide) {
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
    }

    if (verb_show) {
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

    // todo: config object for LA (copy sump???)
    // pass config to logicanalyzer.c
    bool has_info = cmdln_args_find_flag('i' | 0x20); // info: show current settings
    command_var_t arg;
    uint32_t oversample;
    bool has_oversample = cmdln_args_find_flag_uint32('o', &arg, &oversample); // oversample: set oversample rate
    uint32_t frequency;
    bool has_frequency = cmdln_args_find_flag_uint32('f', &arg, &frequency); // frequency: set sample rate
    uint32_t debug_level;
    bool has_debug = cmdln_args_find_flag_uint32('d', &arg, &debug_level); // debug: set debug level
    char low_char[3];
    bool has_low_char = cmdln_args_find_flag_string('0', &arg, sizeof(low_char), low_char); // low: set low char
    char high_char[3];
    bool has_high_char = cmdln_args_find_flag_string('1', &arg, sizeof(high_char), high_char); // high: set high char
    uint32_t base_channel;
    bool has_base_channel = cmdln_args_find_flag_uint32('b', &arg, &base_channel); // base channel: set base channel

    bool has_ok;

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
        ui_help_show(true, usage, count_of(usage), options, count_of(options));
        return;
    }

    if (has_info || has_oversample || has_frequency) {
        printf("\r\nLogic Analyzer settings\r\n");
        printf(" Oversample rate: %d\r\n", fala_config.oversample);
        printf(" Sample frequency: %dHz\r\n", fala_config.base_frequency);
        if (oversample != 1) {
            printf("\r\nNote: oversample rate is not 1\r\n");
            printf("Actual sample frequency: %dHz (%d * %dHz)\r\n",
                   fala_config.base_frequency * fala_config.oversample,
                   fala_config.oversample,
                   fala_config.base_frequency);
        }
    }
}
