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
#include "pirate/pullup.h"
#include "pirate/bio.h"
#include "pirate/psu.h"

static const char* const usage[] = {
    "demonstrate hardware buys",
    "bug e9"
};

static const struct ui_help_options options[] = {
    { 0, "-h", T_HELP_FLAG },
};

void bug_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), options, count_of(options))) {
        return;
    }

    char bug_str[3];
    bool verb_e9 = false;
    if (cmdln_args_string_by_position(1, sizeof(bug_str), bug_str)) {
        if (strcmp(bug_str, "e9") == 0) {
            verb_e9 = true;
        }
    }

    if(verb_e9) {

        printf("Repeat bug E9\r\n");
        uint32_t vout, isense, vreg;
        bool fuse;
        psu_measure(&vout, &isense, &vreg, &fuse); 
        if(vout<800){
            printf("Enable a power supply with the 'W' command.\r\n");
            return;
        }
        //gpio_disable_pulls(pin_scl);
        printf("Making IO0 buffer and GPIO input\r\n");
        bio_input(BIO0);
        printf("Enabling Bus Pirate pull-ups\r\n");
        pullup_enable();
        printf("Making IO0 buffer an output\r\n");
        bio_buf_output(BIO0);
        printf("Disabling Bus Pirate pull-ups\r\n");
        pullup_disable();
        busy_wait_ms(10);
        printf("GPIO pin should be 0: %d\r\n", bio_get(BIO0));
        if(bio_get(BIO0)){
            printf("GPIO is 1, E9 found\n");
        }
    }
}
