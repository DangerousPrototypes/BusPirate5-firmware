#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "opt_args.h"
#include "command_attributes.h"
#include "commands.h"
#include "mode/hiz.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_info.h"
#include "ui/ui_help.h"
#include "ui/ui_config.h"
#include "ui/ui_mode.h"
#include "ui/ui_display.h"
#include "pwm.h"
#include "freq.h"
#include "adc.h"
#include "helpers.h"
#include "storage.h"
#include "flash.h"
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"
#include "commands/global/cmd_mcu.h"
#include "commands/global/l_bitorder.h"
#include "commands/global/cmd_convert.h"
#include "commands/global/pause.h"
#include "commands/global/h_help.h"
#include "commands/global/cmd_selftest.h"
#include "commands/global/a_auxio.h"
#include "mode/logicanalyzer.h"
#include "dummy.h"

// command configuration
const struct _command_struct commands[]=
{   //HiZ? Function Help
    {"ls",true,&list_dir,T_CMDLN_LS}, //ls
    {"cd", true, &change_dir,T_CMDLN_CD},//cd
    {"mkdir", true, &make_dir, T_CMDLN_MKDIR},//mkdir
    {"rm", true, &storage_unlink, T_CMDLN_RM}, //rm
    {"cat", true, &cat, T_CMDLN_CAT}, //cat
    {"m", true, &ui_mode_enable_args, T_CMDLN_MODE},            // "m"   //needs trailing int32   
    {"W", false, &psucmd_enable_handler, 0x00},// "W"   T_CMDLN_PSU_EN  //TOD0: more flexability on help handling and also a general deescription
    {"#", true, &cmd_mcu_reset_handler, T_CMDLN_RESET},// "#" 
    {"$", true, &cmd_mcu_jump_to_bootloader_handler,0x00 },     // "$" T_CMDLN_BOOTLOAD
    {"=", true, &cmd_convert_base_handler, T_CMDLN_INT_FORMAT}, // "="
    {"|", true, &cmd_convert_inverse_handler, T_CMDLN_INT_INVERSE}, // "|"   
    {"?", true, &help_handler, 0x00},        // "?" T_CMDLN_HELP
    {"c", true, &ui_config_main_menu, T_CMDLN_CONFIG_MENU},       // "c"
    {"f", true, &freq_single, T_CMDLN_FREQ_ONE},               // "f"    
    {"F", true, &freq_cont, T_CMDLN_FREQ_CONT}, // "F"
    {"G", false, &pwm_configure_enable, T_CMDLN_PWM_CONFIG}, //G
    {"g", false, &pwm_configure_disable, T_CMDLN_PWM_DIS },       // "g"
    {"h", true, &help_handler, 0x00 },         // "h" T_CMDLN_HELP_MODE
    {"hd", true, &helpers_display_help, T_CMDLN_HELP_DISPLAY },     // "hd"
    {"i", true, &ui_info_print_info, T_CMDLN_INFO },               // "i"
    {"l", true, &bitorder_msb_handler, T_CMDLN_BITORDER_MSB },    // "l"
    {"L", true, &bitorder_lsb_handler, T_CMDLN_BITORDER_LSB },    // "L"
    {"o", true, &ui_mode_int_display_format, T_CMDLN_DISPLAY_FORMAT }, // "o"
    {"P", false, &pullups_enable_handler, 0x00 },            // "P" //T_CMDLN_PULLUPS_EN
    {"p", false, &pullups_disable_handler, 0x00 },          // "p" //T_CMDLN_PULLUPS_DIS
    {"w", false, &psucmd_disable_handler, 0x00 },                  // "w" T_CMDLN_PSU_DIS
    {"V", true, &adc_measure_cont, T_CMDLN_ADC_CONT },             // "V"
    {"v", true, &adc_measure_single, T_CMDLN_ADC_ONE },            // "v"
    {"~", true, &cmd_selftest_handler, 0x00},             // "~" selftest T_CMDLN_SELFTEST 
    {"@", true, &auxio_input_handler,0x00},               // "@" T_CMDLN_AUX_IN
    {"a", false, &auxio_low_handler, 0x00},               // "a" T_CMDLN_AUX_LOW
    {"A", false, &auxio_high_handler,0x00},             // "A"T_CMDLN_AUX_HIGH
    {"format", true, &storage_format, T_HELP_CMD_FORMAT },               // "format"
    {"d", true, &ui_display_enable_args, T_CMDLN_DISPLAY },         // "d" 
    {"logic", true, &la_test_args, T_CMDLN_LOGIC },                     // "logic" 
    {"hex", true, &hex, T_CMDLN_HEX },                                // "hex"
    {"pause", true, &pause_handler, T_HELP_CMD_PAUSE },             // "pause"
    {"flash", true, &flash, 0x00 },                              // "dump"
    {"dummy", true, &dummy_func, 0x00 },                              // "dummy"
    {"help", true, &help_handler, 0x00},
};

const uint32_t commands_count=count_of(commands);