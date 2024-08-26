#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "binmode/binmodes.h"
#include "system_config.h"
#include "binmode/sump.h"
#include "binmode/dirtyproto.h"
#include "binmode/legacy4third.h"
#include "lib/arduino-ch32v003-swio/arduino_ch32v003.h"

void binmode_null_func(void) {
    return;
}

const binmode_t binmodes[]={
    {   &sump_logic_analyzer_setup,
        &sump_logic_analyzer_service,
        &sump_logic_analyzer_cleanup,  
        sump_logic_analyzer_name,  
    },
    {   &binmode_null_func,
        &dirtyproto_mode,
        &binmode_null_func, 
        dirtyproto_mode_name,   
    },
    {
        &binmode_null_func,
        &arduino_ch32v003,
        &binmode_null_func, 
        arduino_ch32v003_name,   
    },
    {
        &binmode_null_func,
        &legacy4third_mode,
        &binmode_null_func, 
        legacy4third_mode_name,   
    },
};

inline void binmode_setup(void){
    binmodes[system_config.binmode_select].binmode_setup();
}

inline void binmode_service(void){
    binmodes[system_config.binmode_select].binmode_service();
}

inline void binmode_cleanup(void){
    binmodes[system_config.binmode_select].binmode_cleanup();
}