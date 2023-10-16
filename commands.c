#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "commands.h"
#include "mode/hiz.h"
#include "auxpinfunc.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_info.h"
#include "ui/ui_config.h"
#include "ui/ui_mode.h"
#include "pwm.h"
#include "freq.h"
#include "adc.h"
#include "psu.h"
#include "pullups.h"
#include "helpers.h"
#include "storage.h"
#include "dump.h"
#include "mcu/rp2040.h"

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
    [CMD_LOAD]="load"    
};
static_assert(count_of(cmd)==CMD_LAST_ITEM_ALWAYS_AT_THE_END, "Command array wrong length");

const uint32_t count_of_cmd=count_of(cmd);

char ls_help[]="ls {directory} - list files in the current location or {directory} location.";
char no_help[]="Help not currently available for this command.";

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
const struct _parsers m_parsers[]={{&ui_parse_get_string},{NULL},{NULL},{NULL},{NULL}};
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

const struct _command_parse exec_new[]=
{
    {
        true,
        &list_dir,
        &list_dir_parsers[0],
        &ls_help[0],
    }, 
    {
        true, 
        &change_dir,
        &change_dir_parsers[0],
        &no_help[0],
    }, // CMD_CD 
    {
        true, 
        &make_dir,
        &make_dir_parsers[0],
        &no_help[0]
    }, // CMD_MKDIR
    {
        true, 
        &storage_unlink,
        &unlink_dir_parsers[0],
        &no_help[0]
    }, // CMD_RM
    {
        true, 
        &cat,
        &cat_dir_parsers[0],
        &no_help[0]
    }, //CMD_CAT
    {
        true, 
        &ui_mode_enable_args,
        &m_parsers[0],
        &no_help[0]
    },            // "m"    
    {
        false, 
        &psu_enable,
        &psuen_parsers[0],
        &no_help[0]
    },                // "W"    
    {
        true, 
        &helpers_mcu_reset,
        0,
        &no_help[0]
    }, // "#" 
    {
        true, 
        &helpers_mcu_jump_to_bootloader,
        0,
        &no_help[0]
    },     // "$" 
    {
        true, 
        &helpers_show_int_formats,
        &show_int_formats_parsers[0],
        &no_help[0]
    }, // "="
    {
        true, 
        &helpers_show_int_inverse,
        &show_int_inverse_parsers[0],
        &no_help[0]
    }, // "|"   
    {
        true, 
        &ui_info_print_help,
        0,
        &no_help[0]
    },        // "?"
    {
        true, 
        &ui_config_main_menu,
        0,
        &no_help[0]
    },       // "c"
    {
        true, 
        &freq_single,
        &freq_single_parsers[0],
        &no_help[0]
    },               // "f"    
    {
        true, 
        &freq_cont,
        &freq_cont_parsers[0],
        &no_help[0]
    },                 // "F"
    {
        false, 
        &pwm_configure_enable,
        &pwmen_parsers[0],
        &no_help[0]
    },     // "G"
    {
        false, 
        &pwm_configure_disable,
        &pwmdis_parsers[0],
        &no_help[0]
    },    // "g"    
    {
        false, 
        &helpers_mode_help,
        0,
        &no_help[0]
    },         // "h"
    {
        true, 
        &ui_info_print_info,
        0,
        &no_help[0]
    },        // "i"
    {
        true, 
        &helpers_bit_order_msb,
        0,
        &no_help[0]
    },     // "l"    
    {
        true, 
        &helpers_bit_order_lsb,
        0,
        &no_help[0]
    },     // "L"
    {
        true, 
        &ui_mode_int_display_format,
        0,
        &no_help[0]
    }, // "o"
    {
        false, 
        &pullups_enable,
        0,
        &no_help[0]
    },           // "P"    
    {
        false, 
        &pullups_disable,
        0,
        &no_help[0]
    },          // "p"    
    {
        false, 
        &psu_disable,
        0,
        &no_help[0]
    },              // "w"    
    {
        true, 
        &adc_measure_cont,
        &adc_cont_parsers[0],
        &no_help[0]
    },          // "V"
    {
        true, 
        &adc_measure_single,
        &adc_single_parsers[0],
        &no_help[0]
    },        // "v"    
    {
        true, 
        &helpers_selftest,
        0,
        &no_help[0]
    },           // "~" selftest    
    {
        true, 
        &auxpinfunc_input,
        &aux_input_parsers[0],
        &no_help[0]
    },        // "@"    
    {
        false, 
        &auxpinfunc_low,
        &aux_low_parsers[0],
        &no_help[0]
    },        // "a"    
    {
        false, 
        &auxpinfunc_high,
        &aux_high_parsers[0],
        &no_help[0]
    },        // "A"                
    {
        true, 
        &dump,
        &dump_parsers[0],
        &no_help[0]
    },        // "dump"   
    {
        true, 
        &load,
        &load_parsers[0],
        &no_help[0]
    }      // "load"   
         
};