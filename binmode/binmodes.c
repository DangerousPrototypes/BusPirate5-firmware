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
const binmode_t binmodes[] = {
    {
        .lock_terminal = false,
        .binmode_name = sump_logic_analyzer_name,
        .binmode_setup = sump_logic_analyzer_setup,
        .binmode_service = sump_logic_analyzer_service,
        .binmode_cleanup = sump_logic_analyzer_cleanup,
    },
    {
        .lock_terminal = true,
        .binmode_name = dirtyproto_mode_name,
        .binmode_setup = binmode_null_func_void,
        .binmode_service = dirtyproto_mode,
        .binmode_cleanup = binmode_null_func_void,
    },
    {
        .lock_terminal = true,
        .binmode_name = arduino_ch32v003_name,
        .binmode_setup = binmode_null_func_void,
        .binmode_service = arduino_ch32v003,
        .binmode_cleanup = arduino_ch32v003_cleanup,
    },
    {
        .lock_terminal = false,
        .binmode_name = falaio_name,
        .binmode_setup = falaio_setup,
        .binmode_service = falaio_service,
        .binmode_cleanup = falaio_cleanup,
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