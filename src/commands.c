#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "command_struct.h"
#include "commands.h"
#include "mode/hiz.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_info.h"
#include "ui/ui_help.h"
#include "ui/ui_config.h"
#include "ui/ui_mode.h"
#include "ui/ui_display.h"
#include "commands/global/pwm.h"
#include "commands/global/freq.h"
#include "commands/global/a_auxio.h"
#include "commands/global/v_adc.h"
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"
#include "commands/global/cmd_mcu.h"
#include "commands/global/l_bitorder.h"
#include "commands/global/cmd_convert.h"
#include "commands/global/pause.h"
#include "commands/global/h_help.h"
#include "commands/global/cmd_selftest.h"
#include "commands/global/a_auxio.h"
#include "commands/global/dummy.h"
#include "commands/global/disk.h"
#include "commands/global/i_info.h"
#include "commands/global/macro.h"
#include "commands/global/script.h"
#include "commands/global/tutorial.h"
#include "commands/global/button_scr.h"
#include "commands/global/smps.h"
#include "commands/global/cls.h"
#include "commands/global/logic.h"
#include "commands/global/cmd_binmode.h"
#include "commands/global/bug.h"
#include "commands/global/image.h"
#include "commands/global/dump.h"
#if BP_VER !=5
#include "commands/global/otpdump.h"
#include "commands/global/otp.h"
#endif
#include "commands/global/ovrclk.h"
#include "commands/global/cert.h"

/*
struct ui_help_topics {
    uint8_t topic_id;
    const char* description;
};

enum {
    HELP_TOPIC_IO = 0,
    HELP_TOPIC_CONFIGURE,
    HELP_TOPIC_SYSTEM,
    HELP_TOPIC_FILES,
    HELP_TOPIC_SCRIPT,
    HELP_TOPIC_TOOLS,
    HELP_TOPIC_MODE,
    HELP_TOPIC_SYNTAX,
};

const struct ui_help_topics help_topics[] = {
    { HELP_TOPIC_IO, T_HELP_SECTION_IO},
    { HELP_TOPIC_CONFIGURE, T_HELP_SECTION_CONFIGURE},
    { HELP_TOPIC_SYSTEM, T_HELP_SECTION_SYSTEM},
    { HELP_TOPIC_FILES, T_HELP_SECTION_FILES},
    { HELP_TOPIC_SCRIPT, T_HELP_SECTION_SCRIPT},
    { HELP_TOPIC_TOOLS, T_HELP_SECTION_TOOLS},
    { HELP_TOPIC_MODE, T_HELP_SECTION_MODE},
    { HELP_TOPIC_SYNTAX, T_HELP_SECTION_SYNTAX},
};

const struct 
*/

// command configuration
const struct _global_command_struct commands[] = {
    // clang-format off
{ .command="ls",        .allow_hiz=true,  .func=&disk_ls_handler,                    .help_text=0x00 }, // ls T_CMDLN_LS
{ .command="cd",        .allow_hiz=true,  .func=&disk_cd_handler,                    .help_text=0x00 }, // cd T_CMDLN_CD
{ .command="mkdir",     .allow_hiz=true,  .func=&disk_mkdir_handler,                 .help_text=0x00 }, // mkdir T_CMDLN_MKDIR
{ .command="rm",        .allow_hiz=true,  .func=&disk_rm_handler,                    .help_text=0x00 }, // rm T_CMDLN_RM
{ .command="cat",       .allow_hiz=true,  .func=&disk_cat_handler,                   .help_text=0x00 }, // cat T_CMDLN_CAT
{ .command="m",         .allow_hiz=true,  .func=&ui_mode_enable_args,                .help_text=T_CMDLN_MODE }, // "m"   //needs trailing int32
{ .command="W",         .allow_hiz=false, .func=&psucmd_enable_handler,              .help_text=0x00 }, // "W"   T_CMDLN_PSU_EN  //TOD0: more flexability on help handling and also a general deescription
{ .command="#",         .allow_hiz=true,  .func=&cmd_mcu_reset_handler,              .help_text=T_CMDLN_RESET }, // "#"
{ .command="$",         .allow_hiz=true,  .func=&cmd_mcu_jump_to_bootloader_handler, .help_text=0x00 }, // "$" T_CMDLN_BOOTLOAD
{ .command="=",         .allow_hiz=true,  .func=&cmd_convert_base_handler,           .help_text=T_CMDLN_INT_FORMAT }, // "="
{ .command="|",         .allow_hiz=true,  .func=&cmd_convert_inverse_handler,        .help_text=T_CMDLN_INT_INVERSE }, // "|"
{ .command="?",         .allow_hiz=true,  .func=&help_handler,                       .help_text=0x00 }, // "?" T_CMDLN_HELP
{ .command="c",         .allow_hiz=true,  .func=&ui_config_main_menu,                .help_text=T_CMDLN_CONFIG_MENU }, // "c"
{ .command="f",         .allow_hiz=true,  .func=&freq_single,                        .help_text=T_CMDLN_FREQ_ONE }, // "f"
{ .command="F",         .allow_hiz=true,  .func=&freq_cont,                          .help_text=T_CMDLN_FREQ_CONT }, // "F"
{ .command="G",         .allow_hiz=false, .func=&pwm_configure_enable,               .help_text=T_CMDLN_PWM_CONFIG }, // G
{ .command="g",         .allow_hiz=false, .func=&pwm_configure_disable,              .help_text=T_CMDLN_PWM_DIS }, // "g"
{ .command="h",         .allow_hiz=true,  .func=&help_handler,                       .help_text=0x00 }, // "h" T_CMDLN_HELP_MODE
{ .command="i",         .allow_hiz=true,  .func=&i_info_handler,                     .help_text=T_CMDLN_INFO }, // "i"
{ .command="l",         .allow_hiz=true,  .func=&bitorder_msb_handler,               .help_text=T_CMDLN_BITORDER_MSB }, // "l"
{ .command="L",         .allow_hiz=true,  .func=&bitorder_lsb_handler,               .help_text=T_CMDLN_BITORDER_LSB }, // "L"
{ .command="o",         .allow_hiz=true,  .func=&ui_mode_int_display_format,         .help_text=T_CMDLN_DISPLAY_FORMAT }, // "o"
{ .command="P",         .allow_hiz=false, .func=&pullups_enable_handler,             .help_text=0x00 }, // "P" //T_CMDLN_PULLUPS_EN
{ .command="p",         .allow_hiz=false, .func=&pullups_disable_handler,            .help_text=0x00 }, // "p" //T_CMDLN_PULLUPS_DIS
{ .command="w",         .allow_hiz=false, .func=&psucmd_disable_handler,             .help_text=0x00 }, // "w" T_CMDLN_PSU_DIS
{ .command="V",         .allow_hiz=true,  .func=&adc_measure_cont,                   .help_text=0x00 }, // "V" T_CMDLN_ADC_CONT
{ .command="v",         .allow_hiz=true,  .func=&adc_measure_single,                 .help_text=0x00 }, // "v" T_CMDLN_ADC_ONE
{ .command="~",         .allow_hiz=true,  .func=&cmd_selftest_handler,               .help_text=0x00 }, // "~" selftest T_CMDLN_SELFTEST
{ .command="@",         .allow_hiz=true,  .func=&auxio_input_handler,                .help_text=0x00 }, // "@" T_CMDLN_AUX_IN
{ .command="a",         .allow_hiz=false, .func=&auxio_low_handler,                  .help_text=0x00 }, // "a" T_CMDLN_AUX_LOW
{ .command="A",         .allow_hiz=false, .func=&auxio_high_handler,                 .help_text=0x00 }, // "A"T_CMDLN_AUX_HIGH
{ .command="format",    .allow_hiz=true,  .func=&disk_format_handler,                .help_text=0x00 }, // "format" T_HELP_CMD_FORMAT
{ .command="label",     .allow_hiz=true,  .func=&disk_label_handler,                 .help_text=0x00 },
{ .command="d",         .allow_hiz=true,  .func=&ui_display_enable_args,             .help_text=T_CMDLN_DISPLAY }, // "d"
{ .command="logic",     .allow_hiz=true,  .func=&logic_handler,                      .help_text=0x00 }, // "logic"
{ .command="hex",       .allow_hiz=true,  .func=&disk_hex_handler,                   .help_text=0x00 }, // "hex"  T_CMDLN_HEX
{ .command="pause",     .allow_hiz=true,  .func=&pause_handler,                      .help_text=0x00 }, // "pause"
{ .command="dummy",     .allow_hiz=true,  .func=&dummy_handler,                      .help_text=0x00 }, // "dummy"
{ .command="help",      .allow_hiz=true,  .func=&help_handler,                       .help_text=0x00 },
{ .command="macro",     .allow_hiz=true,  .func=&macro_handler,                      .help_text=0x00 },
{ .command="tutorial",  .allow_hiz=true,  .func=&tutorial_handler,                   .help_text=0x00 },
{ .command="script",    .allow_hiz=true,  .func=&script_handler,                     .help_text=0x00 },
{ .command="button",    .allow_hiz=true,  .func=&button_scr_handler,                 .help_text=0x00 },
{ .command="cls",       .allow_hiz=true,  .func=&ui_display_clear,                   .help_text=0x00 },
{ .command="smps",      .allow_hiz=true,  .func=&smps_handler,                       .help_text=0x00 },
{ .command="binmode",   .allow_hiz=true,  .func=&cmd_binmode_handler,                .help_text=0x00 },
{ .command="bug",       .allow_hiz=true,  .func=&bug_handler,                        .help_text=0x00 },
{ .command="image",     .allow_hiz=true,  .func=&image_handler,                      .help_text=0x00 },
{ .command="dump",      .allow_hiz=false, .func=&dump_handler,                       .help_text=0x00 },
#if BP_VER != 5
{ .command="otpdump",   .allow_hiz=true,  .func=&otpdump_handler,                    .help_text=0x00 },
{ .command="otp",       .allow_hiz=true,  .func=&otp_handler,                        .help_text=0x00 },
#endif
{ .command="ovrclk",    .allow_hiz=true,  .func=&ovrclk_handler,                     .help_text=0x00 },
{ .command="cert",      .allow_hiz=true,  .func=&cert_handler,                       .help_text=0x00 },
    // clang-format on
};

const uint32_t commands_count = count_of(commands);
