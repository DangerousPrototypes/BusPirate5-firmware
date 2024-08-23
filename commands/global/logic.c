#include <stdio.h>
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

static const char* const usage[] = {
    "logic analyzer setup"
    "[-i] [-g] [-o oversample] [-f frequency] [-d debug]",
    "-i info, -g interactive graph, -o oversample rate, -f sample frequency (raw)"
    "-d debug level: 0-2, 2 shows graph after capture",
};

static const struct ui_help_options options[] = {
    { 0, "-h", T_HELP_FLAG },
};

void logic_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options))) {
        return;
    }

    // todo: config object for LA (copy sump???)
    // pass config to logicanalyzer.c
    bool has_info = cmdln_args_find_flag('i' | 0x20);  // info: show current settings
    bool has_graph = cmdln_args_find_flag('g' | 0x20); // graph: interactive logic toolbar
    command_var_t arg;
    uint32_t oversample;
    bool has_oversample = cmdln_args_find_flag_uint32('o', &arg, &oversample); // oversample: set oversample rate
    uint32_t frequency;
    bool has_frequency = cmdln_args_find_flag_uint32('f', &arg, &frequency); // frequency: set sample rate
    uint32_t debug_level;
    bool has_debug = cmdln_args_find_flag_uint32('d', &arg, &debug_level); // debug: set debug level

    bool has_redraw = cmdln_args_find_flag('r' | 0x20); // redraw: redraw the graph

    if(has_redraw) {
        logic_bar_redraw(0, 0);
        return;
    }

    if (has_debug) {
        if(debug_level > 2) {
            printf("Error: debug level must be 0, 1, or 2, '%d' is invalid\r\n", debug_level);
            res->error = true;
            return;
        }
        printf("Debug level set to: %d\r\n", debug_level);
        // update fala config struct
        fala_config.debug_level = debug_level;
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
    }

    if (has_frequency) {
        printf("Sample frequency set to: %dHz\r\n", frequency);
        // update fala config struct
        fala_config.base_frequency = frequency;
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

    if (has_graph) {
        // start graph
        logic_bar();
    }
}
