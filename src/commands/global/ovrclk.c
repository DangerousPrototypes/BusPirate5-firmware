// TODO: BIO use, pullups, psu
/*
    Welcome to dummy.c, a growing demonstration of how to add commands to the Bus Pirate firmware.
    You can also use this file as the basis for your own commands.
    Type "dummy" at the Bus Pirate prompt to see the output of this command
    Temporary info available at https://forum.buspirate.com/t/command-line-parser-for-developers/235
*/
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
//#include "fatfs/ff.h"       // File system related
//#include "pirate/storage.h" // File system related
#include "lib/bp_args/bp_cmd.h"
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
//#include "pirate/amux.h"   // Analog voltage measurement functions
//#include "pirate/button.h" // Button press functions
//#include "msc_disk.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

/*****************************************/
// OVERCLOCKING CAN DESTROY YOUR HARDWARE
// USE AT YOUR OWN RISK
// NO WARRANTY IS PROVIDED
// NO LIABILITY IS ACCEPTED
// NO SUPPORT IS PROVIDED
// The command does nothing unless 
// compiled with BP_OVERCLOCK_ENABLED
//
// #define BP_OVERCLOCK_ENABLED
/*****************************************/
// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "ovrclk \t[-m <MHz> | -k <kHz>] [-v <core mV>]",
                                     "Overclock:%s ovrclk -m 135",
                                     "Change core voltage:%s ovrclk -v 1150 (850-1300mV valid)",};

static const bp_command_opt_t ovrclk_opts[] = {
    { "mhz",  'm', BP_ARG_REQUIRED, "MHz", 0x00 },
    { "khz",  'k', BP_ARG_REQUIRED, "kHz", 0x00 },
    { "volt", 'v', BP_ARG_REQUIRED, "mV",  0x00 },
    { 0 }
};

const bp_command_def_t ovrclk_def = {
    .name = "ovrclk",
    .description = 0x00,
    .opts = ovrclk_opts,
    .usage = usage,
    .usage_count = count_of(usage)
};

void ovrclk_handler(struct command_result* res) {
    #ifndef BP_OVERCLOCK_ENABLED
    printf("!!ovrclk is in demonstration mode!!\r\n");
    printf("To enable overclocking, recompile with BP_OVERCLOCK_ENABLED defined\r\n\r\n");
    #else
    printf("!!Changing clock speed or core voltage may damage your device!!!\r\n");
    printf("!!Use at your own risk!!!\r\n\r\n");
    #endif
    
    if (bp_cmd_help_check(&ovrclk_def, res->help_flag)) {
        return;
    }

    uint32_t v_value;
    uint32_t m_value; // somewhere to keep an integer value
    uint32_t k_value; // somewhere to keep an integer value
    
    bool v_flag = bp_cmd_get_uint32(&ovrclk_def, 'v', &v_value);
    bool m_flag = bp_cmd_get_uint32(&ovrclk_def, 'm', &m_value);
    bool k_flag = bp_cmd_get_uint32(&ovrclk_def, 'k', &k_value);

    /*enum vreg_voltage {
        VREG_VOLTAGE_0_85 = 0b0110,    ///< 0.85v
        VREG_VOLTAGE_0_90 = 0b0111,    ///< 0.90v
        VREG_VOLTAGE_0_95 = 0b1000,    ///< 0.95v
        VREG_VOLTAGE_1_00 = 0b1001,    ///< 1.00v
        VREG_VOLTAGE_1_05 = 0b1010,    ///< 1.05v
        VREG_VOLTAGE_1_10 = 0b1011,    ///< 1.10v
        VREG_VOLTAGE_1_15 = 0b1100,    ///< 1.15v
        VREG_VOLTAGE_1_20 = 0b1101,    ///< 1.20v
        VREG_VOLTAGE_1_25 = 0b1110,    ///< 1.25v
        VREG_VOLTAGE_1_30 = 0b1111,    ///< 1.30v

        VREG_VOLTAGE_MIN = VREG_VOLTAGE_0_85,      ///< Always the minimum possible voltage
        VREG_VOLTAGE_DEFAULT = VREG_VOLTAGE_1_10,  ///< Default voltage on power up.
        VREG_VOLTAGE_MAX = VREG_VOLTAGE_1_30,      ///< Always the maximum possible voltage
    };*/

    
    // now set clock frequency
    if(m_flag && k_flag){
        printf("Invalid options, -m and -k both set\r\n");
        return;
    }

    if(v_flag){
        //0.85 = 0b0110 = 6
        //1.30 = 0b1111 = 15
        //calculate the voltage value
        if(v_value < 850 || v_value > 1300){
            printf("Invalid core voltage value, valid range is 850-1300mV\r\n");
            return;
        }
        uint32_t vreg_value = ((v_value - 850) /50)+6;
        // calculate actual voltage
        uint32_t vreg_actual_mv = ((vreg_value-6) * 50) + 850;
        printf("Setting core voltage to %dmV (0b%04b)\r\n", vreg_actual_mv, vreg_value);
        #ifdef BP_OVERCLOCK_ENABLED
        vreg_set_voltage(vreg_value);
        #endif
    }
    
    if (m_flag) {
        printf("Setting clock speed to %dMHz\r\n", m_value);
        k_value = m_value * 1000;
    }
    if (k_flag) {
        printf("Setting clock speed to %dkHz\r\n", k_value);
    } 

    if(m_flag || k_flag){
        bool success = true;
        #ifdef BP_OVERCLOCK_ENABLED
            success = set_sys_clock_khz(k_value, false);
        #endif
        if(!success){
            printf("Nearest clock frequency could not be calculated\r\n");
        }else{
            printf("Clock speed changed\r\n");
        }
    }

    if(!m_flag && !k_flag && !v_flag){
        printf("Nothing to do\r\n");
        bp_cmd_help_show(&ovrclk_def);
    }

}