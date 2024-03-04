#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "opt_args.h"
#include "command_attributes.h"
#include "commands.h"
#include "mode/hiz.h"
#include "auxpinfunc.h"
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
#include "psu.h"
#include "pullups.h"
#include "helpers.h"
#include "storage.h"
#include "dump.h"
#include "mcu/rp2040.h"
#include "mode/logicanalyzer.h"

// command configuration
const struct _command_struct commands[]=
{   //HiZ? Function Help
    {"ls",true,&list_dir,T_CMDLN_LS}, //ls
    {"cd", true, &change_dir,T_CMDLN_CD},//cd
    {"mkdir", true, &make_dir, T_CMDLN_MKDIR},//mkdir
    {"rm", true, &storage_unlink, T_CMDLN_RM}, //rm
    {"cat", true, &cat, T_CMDLN_CAT}, //cat
    {"m", true, &ui_mode_enable_args, T_CMDLN_MODE},            // "m"   //needs trailing int32   
    {"W", false, &psu_enable, T_CMDLN_PSU_EN},// "W"    
    {"#", true, &helpers_mcu_reset, T_CMDLN_RESET},// "#" 
    {"$", true, &helpers_mcu_jump_to_bootloader, T_CMDLN_BOOTLOAD},     // "$" 
    {"=", true, &helpers_show_int_formats, T_CMDLN_INT_FORMAT}, // "="
    {"|", true, &helpers_show_int_inverse, T_CMDLN_INT_INVERSE}, // "|"   
    {"?", true, &ui_help_print_args, T_CMDLN_HELP},        // "?"
    {"c", true, &ui_config_main_menu, T_CMDLN_CONFIG_MENU},       // "c"
    {"f", true, &freq_single, T_CMDLN_FREQ_ONE},               // "f"    
    {"F", true, &freq_cont, T_CMDLN_FREQ_CONT}, // "F"
    {"G", false, &pwm_configure_enable, T_CMDLN_PWM_CONFIG}, //G
    {"g", false, &pwm_configure_disable, T_CMDLN_PWM_DIS },       // "g"
    {"h", false, &helpers_mode_help, T_CMDLN_HELP_MODE },         // "h"
    {"hd", true, &helpers_display_help, T_CMDLN_HELP_DISPLAY },     // "hd"
    {"i", true, &ui_info_print_info, T_CMDLN_INFO },               // "i"
    {"l", true, &helpers_bit_order_msb, T_CMDLN_BITORDER_MSB },    // "l"
    {"L", true, &helpers_bit_order_lsb, T_CMDLN_BITORDER_LSB },    // "L"
    {"o", true, &ui_mode_int_display_format, T_CMDLN_DISPLAY_FORMAT }, // "o"
    {"P", false, &pullups_enable, T_CMDLN_PULLUPS_EN },            // "P"
    {"p", false, &pullups_disable, T_CMDLN_PULLUPS_DIS },          // "p"
    {"w", false, &psu_disable, T_CMDLN_PSU_DIS },                  // "w"
    {"V", true, &adc_measure_cont, T_CMDLN_ADC_CONT },             // "V"
    {"v", true, &adc_measure_single, T_CMDLN_ADC_ONE },            // "v"
    {"~", true, &helpers_selftest, T_CMDLN_SELFTEST },             // "~" selftest
    {"@", true, &auxpinfunc_input, T_CMDLN_AUX_IN },               // "@"
    {"a", false, &auxpinfunc_low, T_CMDLN_AUX_LOW },               // "a"
    {"A", false, &auxpinfunc_high, T_CMDLN_AUX_HIGH },             // "A"
    {"format", true, &storage_format, T_HELP_CMD_FORMAT },               // "format"
    {"d", true, &ui_display_enable_args, T_CMDLN_DISPLAY },         // "d" 
    {"logic", true, &la_test_args, T_CMDLN_LOGIC },                     // "logic" 
    {"hex", true, &hex, T_CMDLN_HEX },                                // "hex"
    {"pause", true, &helpers_pause_args, T_HELP_CMD_PAUSE },             // "pause"
    {"flash", true, &flash, T_CMDLN_DUMP },                              // "dump"
};

const uint32_t commands_count=count_of(commands);

const struct _parsers list_dir_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers change_dir_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers make_dir_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers unlink_dir_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers cat_dir_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers m_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers display_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers psuen_parsers[]={{NULL},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers show_int_formats_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers show_int_inverse_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers freq_single_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers freq_cont_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers pwmen_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers pwmdis_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers adc_cont_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers adc_single_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers aux_input_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers aux_low_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers aux_high_parsers[]={{&ui_parse_get_int_args},{NULL},{NULL},{NULL},{NULL}};
const struct _parsers logic_parsers[]={{&ui_parse_get_int_args},{&ui_parse_get_int_args},{&ui_parse_get_int_args},{&ui_parse_get_int_args},{NULL}};
const struct _parsers hex_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
