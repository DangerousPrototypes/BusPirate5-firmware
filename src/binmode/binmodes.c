#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "binmode/binmodes.h"
#include "bytecode.h"
#include "command_struct.h"
#include "modes.h"
//#include "pirate/psu.h"
#include "pirate/pullup.h"
#include "binmode/binio.h"
#include "system_config.h"
#include "binmode/sump.h"
#include "binmode/dirtyproto.h"
#include "binmode/legacy4third.h"
#include "binmode/falaio.h"
#include "binmode/irtoy-irman.h"
#include "binmode/irtoy-air.h"
#include "lib/arduino-ch32v003-swio/arduino_ch32v003.h"
#include "pirate/storage.h" // File system related
#include "usb_rx.h"
#include "ui/ui_const.h"
#include "commands/global/w_psu.h"

void binmode_null_func_void(void) {
    return;
}

const binmode_t binmodes[] = {
    {
        .lock_terminal = false,
        .can_save_config = true,
        .reset_to_hiz = false,
        .pullup_enabled = false,
        .psu_en_voltage = 0,
        .psu_en_current = 0,
        .button_to_exit = false,
        .binmode_name = sump_logic_analyzer_name,
        .binmode_setup = sump_logic_analyzer_setup,
        .binmode_service = sump_logic_analyzer_service,
        .binmode_cleanup = sump_logic_analyzer_cleanup,
    },
    {
        .lock_terminal = true,
        .can_save_config = false,
        .reset_to_hiz = false,
        .pullup_enabled = false,
        .psu_en_voltage = 0,
        .psu_en_current = 0,  
        .button_to_exit = false,      
        .binmode_name = dirtyproto_mode_name,
        .binmode_setup = binmode_null_func_void,
        .binmode_service = dirtyproto_mode,
        .binmode_cleanup = binmode_null_func_void,
    },
    {
        .lock_terminal = true,
        .can_save_config = false,
        .reset_to_hiz = true,
        .pullup_enabled = false,
        .psu_en_voltage = 0,
        .psu_en_current = 0,     
        .button_to_exit = true,   
        .binmode_name = arduino_ch32v003_name,
        .binmode_setup = binmode_null_func_void,
        .binmode_service = arduino_ch32v003,
        .binmode_cleanup = arduino_ch32v003_cleanup,
    },
    {
        .lock_terminal = false,
        .can_save_config = true,
        .reset_to_hiz = false,
        .pullup_enabled = false,
        .psu_en_voltage = 0,
        .psu_en_current = 0, 
        .button_to_exit = false,       
        .binmode_name = falaio_name,
        .binmode_setup = falaio_setup,
        .binmode_setup_message = falaio_setup_message,
        .binmode_service = falaio_service,
        .binmode_cleanup = falaio_cleanup,
    },
    {
        .lock_terminal = true,
        .can_save_config = false,
        .reset_to_hiz = false,
        .pullup_enabled = false,
        .psu_en_voltage = 0,
        .psu_en_current = 0,  
        .button_to_exit = false,      
        .binmode_name = legacy4third_mode_name,
        .binmode_setup = binmode_null_func_void,
        .binmode_service = legacy4third_mode,
        .binmode_cleanup = binmode_null_func_void,
    },
    {
        .lock_terminal = true,
        .can_save_config = false,
        .reset_to_hiz = true,
        .pullup_enabled = false,
        .psu_en_voltage = 5.0,
        .psu_en_current = 0,   
        .button_to_exit = true,     
        .binmode_name = irtoy_irman_name,
        .binmode_setup = irtoy_irman_setup,
        .binmode_cleanup = irtoy_irman_cleanup,
        .binmode_service = irtoy_irman_service,
    },
    {
        .lock_terminal = true,
        .can_save_config = false,
        .binmode_name = irtoy_air_name,
        .reset_to_hiz = true,
        .pullup_enabled = false,
        .psu_en_voltage = 5.0,
        .psu_en_current = 0,  
        .button_to_exit = false,      
        .binmode_setup = irtoy_air_setup,
        .binmode_cleanup = irtoy_air_cleanup,
        .binmode_service = irtoy_air_service,
    },
};

inline void binmode_setup(void) {
    if (binmodes[system_config.binmode_select].lock_terminal) {
        binmode_terminal_lock(true);
    }
    if(binmodes[system_config.binmode_select].button_to_exit) {
        printf("Press any key to exit binary mode\r\n");
    }
    // optional reset to HiZ
    if(binmodes[system_config.binmode_select].reset_to_hiz) {
        modes[system_config.mode].protocol_cleanup();
        system_config.mode = 0;
        modes[system_config.mode].protocol_setup();
    }

    //voltage and current 
    if(binmodes[system_config.binmode_select].psu_en_voltage>0) {
        bool i_override;
        if(binmodes[system_config.binmode_select].psu_en_current==0) {
            i_override = true;
        }
        psucmd_enable(binmodes[system_config.binmode_select].psu_en_voltage, binmodes[system_config.binmode_select].psu_en_current, i_override);
        //system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
        //monitor_clear_current(); // reset current so the LCD gets all characters
     }


    // optional pull-up enable
    if(binmodes[system_config.binmode_select].pullup_enabled) {
        pullup_enable();
    }

    binmodes[system_config.binmode_select].binmode_setup();
}

inline void binmode_service(void) {
    //exit on button press
    if(binmodes[system_config.binmode_select].button_to_exit) {
        char c;
        if(rx_fifo_try_get(&c)){
            binmode_cleanup();
            system_config.binmode_select = 0;
            binmode_setup();
            system_config.mode = 0;
            modes[system_config.mode].protocol_setup();
            return;
        }
    }
    binmodes[system_config.binmode_select].binmode_service();
}

inline void binmode_cleanup(void) {
    binmodes[system_config.binmode_select].binmode_cleanup();
    if (binmodes[system_config.binmode_select].lock_terminal) {
        binmode_terminal_lock(false);
    }

    if(binmodes[system_config.binmode_select].psu_en_voltage>0) {
        //psu_disable();
        psucmd_disable();
    }

    if(binmodes[system_config.binmode_select].pullup_enabled) {
        pullup_disable();
    }
}

void binmode_load_save_config(bool save) {
    const char config_file[] = "bpbinmod.bp";
    uint32_t binmode_select = system_config.binmode_select;
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.binmode", &binmode_select, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format off
    };
    if (save) {
        storage_save_mode(config_file, config_t, count_of(config_t));
    } else {
        storage_load_mode(config_file, config_t, count_of(config_t));
        system_config.binmode_select = binmode_select;
    }
}