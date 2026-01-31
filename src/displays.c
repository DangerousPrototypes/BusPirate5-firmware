/**
 * @file displays.c
 * @brief Display mode management and dispatch table
 * 
 * This file implements the display system for Bus Pirate, managing different
 * LCD display modes including:
 * - Default mode: Shows pin voltages and labels
 * - Scope mode: Real-time oscilloscope display
 * - Disabled mode: Screen off for power saving
 * 
 * Each display mode provides setup, cleanup, settings, and update callbacks
 * for managing the 240x320 IPS LCD.
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "command_struct.h"
#include "commands.h"
#include "displays.h"

#include "display/default.h"
#include "display/disabled.h"
#ifdef BP_USE_SCOPE
#include "display/scope.h"
#endif

/**
 * @brief Display mode dispatch table
 * 
 * This table defines all available LCD display modes and their callbacks.
 * Each entry specifies:
 * - Periodic service function (for polling/updates)
 * - Setup and cleanup functions
 * - Settings display function
 * - Help text function
 * - Friendly name for UI
 * - Mode-specific commands
 * - LCD update function
 * 
 * @note MAXDISPLAY is defined in displays.h
 */

struct _display displays[MAXDISPLAY] = {
    {
        noperiodic,              // service to regular poll whether a byte ahs arrived
        disp_default_setup,      // setup UI
        disp_default_setup_exc,  // real setup
        disp_default_cleanup,    // cleanup for HiZ
        disp_default_settings,   // display settings
        0,                       // display small help about the protocol
        "Default",               // friendly name (promptname)
        0,                       // scope specific commands
        disp_default_lcd_update, // screen write
    },
#ifdef BP_USE_SCOPE
    {
        scope_periodic,   // service to regular poll whether a byte ahs arrived
        scope_setup,      // setup UI
        scope_setup_exc,  // real setup
        scope_cleanup,    // cleanup for HiZ
        scope_settings,   // display settings
        scope_help,       // display small help about the protocol
        "Scope",          // friendly name (promptname)
        scope_commands,   // scope specific commands
        scope_lcd_update, // scope screen write
    },
#endif
    {
        noperiodic,               // service to regular poll whether a byte ahs arrived
        disp_disabled_setup,      // setup UI
        disp_disabled_setup_exc,  // real setup
        disp_disabled_cleanup,    // cleanup for HiZ
        disp_disabled_settings,   // display settings
        0,                        // display small help about the protocol
        "Disabled (Screen Off)",  // friendly name (promptname)
        0,                        // scope specific commands
        disp_disabled_lcd_update, // screen write
    },
};
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
