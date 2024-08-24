#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "binmode/binmodes.h"
#include "binmode/binio.h"
#include "system_config.h"
#include "binmode/sump.h"
#include "binmode/dirtyproto.h"
#include "binmode/falaio.h"
#include "lib/arduino-ch32v003-swio/arduino_ch32v003.h"

void binmode_null_func_void(void) {
    return;
}

//TODO: add setting for lock terminal or not
const binmode_t binmodes[]={
    {   .lock_terminal=false,
        .binmode_name=sump_logic_analyzer_name, 
        &sump_logic_analyzer_setup,
        &sump_logic_analyzer_service,
        &sump_logic_analyzer_cleanup, 
    },
    {   .lock_terminal=true,
        dirtyproto_mode_name,
        &binmode_null_func_void,
        &dirtyproto_mode,
        &binmode_null_func_void, 
    },
    {   .lock_terminal=true,
        arduino_ch32v003_name,
        &binmode_null_func_void,
        &arduino_ch32v003,
        &binmode_null_func_void,             
    },
    {   .lock_terminal=false,
        falaio_name, 
        &falaio_setup,
        &falaio_service,
        &falaio_cleanup, 
    },    
};

inline void binmode_setup(void){
    if(binmodes[system_config.binmode_select].lock_terminal){
        binmode_terminal_lock(true);
    }
    binmodes[system_config.binmode_select].binmode_setup();
}

inline void binmode_service(void){
    binmodes[system_config.binmode_select].binmode_service();
}

inline void binmode_cleanup(void){
    binmodes[system_config.binmode_select].binmode_cleanup();
    if(binmodes[system_config.binmode_select].lock_terminal){
        binmode_terminal_lock(false);
    }
}