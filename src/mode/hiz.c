#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "hiz.h"
#include "pirate/bio.h"
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"
#include "ui/ui_help.h"

const char* hiz_pins(void) {
    return "-\t-\t-\t-\t-\t-\t-\t-";
}

const char* hiz_error(void) {
    return GET_T(T_MODE_ERROR_NO_EFFECT_HIZ);
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
const struct _mode_command_struct hiz_commands[] = {
    /*{ .command="", 
        .func=&function, 
        .description_text=T_MODE_COMMAND_DESCRIPTION, 
        .supress_fala_capture=false
    },*/
};
const uint32_t hiz_commands_count = count_of(hiz_commands);

void hiz_help(void) {
    printf("%sHiZ is a safe mode.\r\nIO pins, power and pull-ups are disabled.\r\n", ui_term_color_info());
    printf("To enter an active mode type 'm' and press enter.\r\n");
    ui_help_mode_commands(hiz_commands, hiz_commands_count);
}
