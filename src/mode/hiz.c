#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "hiz.h"
#include "pirate/bio.h"
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"

const char* hiz_pins(void) {
    return "-\t-\t-\t-\t-\t-\t-\t-";
}

const char* hiz_error(void) {
    return GET_T(T_MODE_ERROR_NO_EFFECT_HIZ);
}

void hiz_settings(void) {
    printf("HiZ ()=()");
}

void hiz_cleanup(void) {}

uint32_t hiz_setup(void) {
    return 1;
}

// this is called duringmode changes; takes care pwm, vpu and psu is turned off, also AUX to input
uint32_t hiz_setup_exec(void) {
    // turn everything off
    bio_init();        // make all pins safe
    psucmd_disable();  // turn off power supply
    pullups_disable(); // deactivate
    system_config.freq_active = 0;
    system_config.pwm_active = 0;
    system_config.aux_active = 0;
    for (int i = 0; i < count_of(bio2bufiopin); i++) {
        system_bio_claim(false, i, BP_PIN_IO, 0);
    }
    return 1;
}

// command configuration
const struct _command_struct hiz_commands[] = {
    // HiZ? Function Help
    // note: for now the allow_hiz flag controls if the mode provides it's own help
    //{"sle4442",false,&sle4442,T_HELP_SLE4442}, // the help is shown in the -h *and* the list of mode apps
};
const uint32_t hiz_commands_count = count_of(hiz_commands);

void hiz_help(void) {
    printf("%sHiZ is a safe mode.\r\nIO pins, power and pull-ups are disabled.\r\n", ui_term_color_info());
    printf("To enter an active mode type 'm' and press enter.\r\n");
    printf("\r\nAvailable mode commands:\r\n");
    for (uint32_t i = 0; i < hiz_commands_count; i++) {
        printf("%s%s%s\t%s%s\r\n",
               ui_term_color_prompt(),
               hiz_commands[i].command,
               ui_term_color_info(),
               hiz_commands[i].help_text ? GET_T(hiz_commands[i].help_text) : "Unavailable",
               ui_term_color_reset());
    }
}
