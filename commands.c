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

enum E_CMD{
    CMD_LS=0,
	CMD_CD,
	CMD_MKDIR,
	CMD_RM,
    CMD_CAT,
    CMD_MODE, //temp
    CMD_PSU_EN, //temp
    CMD_RESET,
    CMD_BOOTLOAD,
    CMD_INT_FORMAT,
    CMD_INT_INVERSE,
    CMD_HELP,
    CMD_CONFIG_MENU,
    CMD_FREQ_ONE,
    CMD_FREQ_CONT,
    CMD_PWM_CONFIG,
    CMD_PWM_DIS,
    CMD_HELP_MODE,
    CMD_HELP_DISPLAY,
    CMD_INFO,
    CMD_BITORDER_MSB,
    CMD_BITORDER_LSB,
    CMD_DISPLAY_FORMAT,
    CMD_PULLUPS_EN,
    CMD_PULLUPS_DIS,
    CMD_PSU_DIS,
    CMD_ADC_CONT,
    CMD_ADC_ONE,
    CMD_SELFTEST, 
    CMD_AUX_IN,
    CMD_AUX_LOW,
    CMD_AUX_HIGH,     
    CMD_DUMP, 
    CMD_LOAD,    
    CMD_FORMAT, 
    CMD_DISPLAY,
    CMD_LOGIC,
    CMD_HEX,
    CMD_LAST_ITEM_ALWAYS_AT_THE_END
};

const char *cmd[]={
	[CMD_LS]="ls",
	[CMD_CD]="cd",
	[CMD_MKDIR]="mkdir",
	[CMD_RM]="rm",
    [CMD_CAT]="cat",
    [CMD_MODE]="m",
    [CMD_PSU_EN]="W",
    [CMD_RESET]="#",
    [CMD_BOOTLOAD]="$",
    [CMD_INT_FORMAT]="=",
    [CMD_INT_INVERSE]="|",
    [CMD_HELP]="?",
    [CMD_CONFIG_MENU]="c",
    [CMD_FREQ_ONE]="f",
    [CMD_FREQ_CONT]="F",
    [CMD_PWM_CONFIG]="G",
    [CMD_PWM_DIS]="g",
    [CMD_HELP_MODE]="h",
    [CMD_HELP_DISPLAY]="hd",
    [CMD_INFO]="i",
    [CMD_BITORDER_MSB]="l",
    [CMD_BITORDER_LSB]="L",
    [CMD_DISPLAY_FORMAT]="o",
    [CMD_PULLUPS_EN]="P",
    [CMD_PULLUPS_DIS]="p",
    [CMD_PSU_DIS]="w",
    [CMD_ADC_CONT]="V",
    [CMD_ADC_ONE]="v",
    [CMD_SELFTEST]="~",
    [CMD_AUX_IN]="@",
    [CMD_AUX_LOW]="a",
    [CMD_AUX_HIGH]="A",
    [CMD_DUMP]="dump",
    [CMD_LOAD]="load",
    [CMD_FORMAT]="format",
    [CMD_DISPLAY]="d",
    [CMD_LOGIC]="logic",
    [CMD_HEX]="hex"
};
static_assert(count_of(cmd)==CMD_LAST_ITEM_ALWAYS_AT_THE_END, "Command array wrong length");

const uint32_t count_of_cmd=count_of(cmd);

bool nullparse(opt_args *result)
{
    busy_wait_at_least_cycles(1);
    return 0;
}

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
const struct _parsers dump_parsers[]={{&ui_parse_get_string},{&ui_parse_get_string},{&ui_parse_get_string},{NULL},{NULL}};
const struct _parsers load_parsers[]={{&ui_parse_get_string},{&ui_parse_get_string},{&ui_parse_get_string},{NULL},{NULL}};
const struct _parsers logic_parsers[]={{&ui_parse_get_int_args},{&ui_parse_get_int_args},{&ui_parse_get_int_args},{&ui_parse_get_int_args},{NULL}};
const struct _parsers hex_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};

const struct _command_parse exec_new[]=
{
    {
        true,
        &list_dir,
        &list_dir_parsers[0],
        T_CMDLN_LS
    }, 
    {
        true, 
        &change_dir,
        &change_dir_parsers[0],
        T_CMDLN_CD,
    }, // CMD_CD 
    {
        true, 
        &make_dir,
        &make_dir_parsers[0],
        T_CMDLN_MKDIR
    }, // CMD_MKDIR
    {
        true, 
        &storage_unlink,
        &unlink_dir_parsers[0],
        T_CMDLN_RM
    }, // CMD_RM
    {
        true, 
        &cat,
        &cat_dir_parsers[0],
        T_CMDLN_CAT
    }, //CMD_CAT
    {
        true, 
        &ui_mode_enable_args,
        &m_parsers[0],
        T_CMDLN_MODE
    },            // "m"    
    {
        false, 
        &psu_enable,
        &psuen_parsers[0],
        T_CMDLN_PSU_EN
    },                // "W"    
    {
        true, 
        &helpers_mcu_reset,
        0,
        T_CMDLN_RESET
    }, // "#" 
    {
        true, 
        &helpers_mcu_jump_to_bootloader,
        0,
        T_CMDLN_BOOTLOAD
    },     // "$" 
    {
        true, 
        &helpers_show_int_formats,
        &show_int_formats_parsers[0],
        T_CMDLN_INT_FORMAT
    }, // "="
    {
        true, 
        &helpers_show_int_inverse,
        &show_int_inverse_parsers[0],
        T_CMDLN_INT_INVERSE
    }, // "|"   
    {
        true, 
        &ui_info_print_help,
        0,
        T_CMDLN_HELP
    },        // "?"
    {
        true, 
        &ui_config_main_menu,
        0,
        T_CMDLN_CONFIG_MENU
    },       // "c"
    {
        true, 
        &freq_single,
        &freq_single_parsers[0],
        T_CMDLN_FREQ_ONE
    },               // "f"    
    {
        true, 
        &freq_cont,
        &freq_cont_parsers[0],
        T_CMDLN_FREQ_CONT
    },                 // "F"
    {
        false, 
        &pwm_configure_enable,
        &pwmen_parsers[0],
        T_CMDLN_PWM_CONFIG
    },     // "G"
    {
        false, 
        &pwm_configure_disable,
        &pwmdis_parsers[0],
        T_CMDLN_PWM_DIS
    },    // "g"    
    {
        false, 
        &helpers_mode_help,
        0,
        T_CMDLN_HELP_MODE
    },         // "h"
    {
        true, 
        &helpers_display_help,
        0,
        T_CMDLN_HELP_DISPLAY
    },         // "hd"
    {
        true, 
        &ui_info_print_info,
        0,
        T_CMDLN_INFO
    },        // "i"
    {
        true, 
        &helpers_bit_order_msb,
        0,
        T_CMDLN_BITORDER_MSB
    },     // "l"    
    {
        true, 
        &helpers_bit_order_lsb,
        0,
        T_CMDLN_BITORDER_LSB
    },     // "L"
    {
        true, 
        &ui_mode_int_display_format,
        0,
        T_CMDLN_DISPLAY_FORMAT
    }, // "o"
    {
        false, 
        &pullups_enable,
        0,
        T_CMDLN_PULLUPS_EN
    },           // "P"    
    {
        false, 
        &pullups_disable,
        0,
        T_CMDLN_PULLUPS_DIS
    },          // "p"    
    {
        false, 
        &psu_disable,
        0,
        T_CMDLN_PSU_DIS
    },              // "w"    
    {
        true, 
        &adc_measure_cont,
        &adc_cont_parsers[0],
        T_CMDLN_ADC_CONT
    },          // "V"
    {
        true, 
        &adc_measure_single,
        &adc_single_parsers[0],
        T_CMDLN_ADC_ONE
    },        // "v"    
    {
        true, 
        &helpers_selftest,
        0,
        T_CMDLN_SELFTEST
    },           // "~" selftest    
    {
        true, 
        &auxpinfunc_input,
        &aux_input_parsers[0],
        T_CMDLN_AUX_IN
    },        // "@"    
    {
        false, 
        &auxpinfunc_low,
        &aux_low_parsers[0],
        T_CMDLN_AUX_LOW
    },        // "a"    
    {
        false, 
        &auxpinfunc_high,
        &aux_high_parsers[0],
        T_CMDLN_AUX_HIGH
    },        // "A"                
    {
        true, 
        &dump,
        &dump_parsers[0],
        T_CMDLN_DUMP
    },        // "dump"   
    {
        true, 
        &load,
        &load_parsers[0],
        T_CMDLN_LOAD
    },      // "load"   
    {
        true, 
        &storage_format,
        0,
        T_CMDLN_NO_HELP
    },   // "format"
    {
        true, 
        &ui_display_enable_args,
        &display_parsers[0],
        T_CMDLN_DISPLAY
    },            // "d"  
    {
        true, 
        &la_test_args,
        &logic_parsers[0],
        T_CMDLN_LOGIC
    },           // "logic"         
    {
        true, 
        &hex,
        &hex_parsers[0],
        T_CMDLN_HEX
    }            // "hex"  
};
