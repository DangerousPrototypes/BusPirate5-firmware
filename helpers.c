#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "modes.h"
#include "displays.h"
#include "display/scope.h"
#include "mode/logicanalyzer.h"

void helpers_display_help(struct command_result *res){
    if (displays[system_config.display].display_help) {
        displays[system_config.display].display_help();
    } else {
        printf("No display help available for this display mode\r\n");
    }
}

void helpers_mode_periodic(){
    displays[system_config.display].display_periodic();
    modes[system_config.mode].protocol_periodic();
    //we need an array with claim/unclaim slots in an array of active utilities
    la_periodic();
}

