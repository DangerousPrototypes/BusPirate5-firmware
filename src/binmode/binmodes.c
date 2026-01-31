/**
 * @file binmodes.c
 * @brief Binary mode management and dispatch table
 * 
 * This file implements the binary mode system for Bus Pirate, which provides
 * direct protocol access without the command-line interface. Binary modes include:
 * - SUMP Logic Analyzer
 * - DirtyProto (legacy Bus Pirate 4 protocol)
 * - Arduino CH32V003 programmer
 * - FALAIO (logic analyzer)
 * - IRtoy modes (IRMAN, AIR)
 * - BPIO (Binary Protocol IO)
 * 
 * Each mode can configure terminal locking, power supply, pullups, and cleanup behavior.
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "binmode/binmodes.h"
#include "bytecode.h"
#include "command_struct.h"
#include "modes.h"
//#include "pirate/psu.h"
#include "pirate/pullup.h"
#include "system_config.h"
#include "binmode/sump.h"
#include "binmode/bpio.h"
#include "binmode/legacy4third.h"
#include "binmode/falaio.h"
#include "binmode/irtoy-irman.h"
#include "binmode/irtoy-air.h"
#include "lib/arduino-ch32v003-swio/arduino_ch32v003.h"
#include "pirate/storage.h" // File system related
#include "usb_rx.h"
#include "ui/ui_const.h"
#include "commands/global/w_psu.h"
#include "ui/ui_term.h"
#include "tusb.h"

/**
 * @brief Lock or unlock the terminal during binary mode operation
 * 
 * When locked, the terminal displays a message and prevents command input.
 * This is used by binary modes that need exclusive access to the USB/UART interface.
 * 
 * @param lock true to lock terminal, false to unlock
 * 
 * @note Only displays messages if USB CDC is connected
 */
void binmode_terminal_lock(bool lock) {
    system_config.binmode_lock_terminal = lock;
    if (!tud_cdc_n_connected(0)) {
        return;
    }
    if (lock) {
        printf("\r\n%sBinmode active. Terminal locked%s\r\n", ui_term_color_info(), ui_term_color_reset());
    } else {
        printf("\r\n%sTerminal unlocked%s\r\n", ui_term_color_info(), ui_term_color_reset());
    }
}

/**
 * @brief Null function placeholder for unused binmode callbacks
 * 
 * Used in the binmode dispatch table when a particular callback
 * is not needed by a specific binary mode.
 */
void binmode_null_func_void(void) {
    return;
}

/**
 * @brief Binary mode dispatch table
 * 
 * This table defines all available binary modes and their configuration:
 * - Terminal locking behavior
 * - Configuration save capability
 * - Mode switching behavior
 * - Power supply and pullup defaults
 * - Setup, service, and cleanup callbacks
 */

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
        .lock_terminal = false,
        .can_save_config = true,
        .reset_to_hiz = false,
        .pullup_enabled = false,
        .psu_en_voltage = 0,
        .psu_en_current = 0,  
        .button_to_exit = false,      
        .binmode_name = dirtyproto_mode_name,
        .binmode_setup = dirtyproto_mode_setup,
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
        psucmd_enable(binmodes[system_config.binmode_select].psu_en_voltage, binmodes[system_config.binmode_select].psu_en_current, i_override, 100);
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