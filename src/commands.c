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
#include "commands/global/dummy.h"
#include "commands/global/disk.h"
#include "commands/global/i_info.h"
#include "commands/global/macro.h"
#include "commands/global/script.h"
#include "commands/global/button_scr.h"
#include "commands/global/smps.h"
#include "commands/global/cls.h"
#include "commands/global/logic.h"
#include "commands/global/cmd_binmode.h"
#include "commands/global/bug.h"
#include "commands/global/image.h"
#include "commands/global/dump.h"
#include "commands/global/hex.h"
#include "commands/global/jep106_lookup.h"
#if RPI_PLATFORM == RP2350
#include "commands/global/otpdump.h"
#endif
#include "commands/global/ovrclk.h"
#include "commands/global/flat.h"

// command configuration
const struct _global_command_struct commands[] = {
    // clang-format off
//                                                                                                                    category
// IO: pin control, power, measurement
{ .command="W",         .allow_hiz=false, .func=&psucmd_enable_handler,              .def=&psucmd_enable_def, .description_text=T_CMDLN_PSU_EN,       .category=CMD_CAT_IO },
{ .command="w",         .allow_hiz=false, .func=&psucmd_disable_handler,             .def=&psucmd_disable_def, .description_text=T_CMDLN_PSU_DIS,      .category=CMD_CAT_IO },
{ .command="P",         .allow_hiz=false, .func=&pullups_enable_handler,             .description_text=T_CMDLN_PULLUPS_EN,   .category=CMD_CAT_IO },
{ .command="p",         .allow_hiz=false, .func=&pullups_disable_handler,            .description_text=T_CMDLN_PULLUPS_DIS,  .category=CMD_CAT_IO },
{ .command="a",         .allow_hiz=false, .func=&auxio_low_handler,                  .def=&auxio_low_def, .description_text=T_CMDLN_AUX_LOW,      .category=CMD_CAT_IO },
{ .command="A",         .allow_hiz=false, .func=&auxio_high_handler,                 .def=&auxio_high_def, .description_text=T_CMDLN_AUX_HIGH,     .category=CMD_CAT_IO },
{ .command="@",         .allow_hiz=true,  .func=&auxio_input_handler,                .def=&auxio_input_def, .description_text=T_CMDLN_AUX_IN,       .category=CMD_CAT_IO },
{ .command="f",         .allow_hiz=true,  .func=&freq_single,                        .def=&freq_single_def, .description_text=T_CMDLN_FREQ_ONE,     .category=CMD_CAT_IO },
{ .command="F",         .allow_hiz=true,  .func=&freq_cont,                          .def=&freq_cont_def, .description_text=T_CMDLN_FREQ_CONT,    .category=CMD_CAT_IO },
{ .command="G",         .allow_hiz=false, .func=&pwm_configure_enable,               .description_text=T_CMDLN_PWM_CONFIG,   .category=CMD_CAT_IO },
{ .command="g",         .allow_hiz=false, .func=&pwm_configure_disable,              .description_text=T_CMDLN_PWM_DIS,      .category=CMD_CAT_IO },
{ .command="v",         .allow_hiz=true,  .func=&adc_measure_single,                 .def=&adc_single_def, .description_text=T_CMDLN_ADC_ONE,      .category=CMD_CAT_IO },
{ .command="V",         .allow_hiz=true,  .func=&adc_measure_cont,                   .def=&adc_cont_def, .description_text=T_CMDLN_ADC_CONT,     .category=CMD_CAT_IO },
// Configure: terminal, display, mode config
{ .command="c",         .allow_hiz=true,  .func=&ui_config_main_menu,                .description_text=T_CMDLN_CONFIG_MENU,  .category=CMD_CAT_CONFIGURE },
{ .command="d",         .allow_hiz=true,  .func=&ui_display_enable_args,             .description_text=T_CMDLN_DISPLAY,      .category=CMD_CAT_CONFIGURE },
{ .command="o",         .allow_hiz=true,  .func=&ui_mode_int_display_format,         .description_text=T_CMDLN_DISPLAY_FORMAT, .category=CMD_CAT_CONFIGURE },
{ .command="l",         .allow_hiz=true,  .func=&bitorder_msb_handler,               .def=&bitorder_msb_def, .description_text=T_CMDLN_BITORDER_MSB, .category=CMD_CAT_CONFIGURE },
{ .command="L",         .allow_hiz=true,  .func=&bitorder_lsb_handler,               .def=&bitorder_lsb_def, .description_text=T_CMDLN_BITORDER_LSB, .category=CMD_CAT_CONFIGURE },
{ .command="cls",       .allow_hiz=true,  .func=&ui_display_clear,                   .def=&cls_def, .description_text=T_HELP_CMD_CLS,       .category=CMD_CAT_CONFIGURE },
// System: info, reboot, selftest
{ .command="i",         .allow_hiz=true,  .func=&i_info_handler,                     .def=&i_info_def, .description_text=T_CMDLN_INFO,         .category=CMD_CAT_SYSTEM },
{ .command="reboot",    .allow_hiz=true,  .func=&cmd_mcu_reboot_handler,             .def=&reboot_def, .description_text=T_CMDLN_REBOOT,       .category=CMD_CAT_SYSTEM },
{ .command="$",         .allow_hiz=true,  .func=&cmd_mcu_jump_to_bootloader_handler, .def=&bootloader_def, .description_text=T_CMDLN_BOOTLOAD,     .category=CMD_CAT_SYSTEM },
{ .command="~",         .allow_hiz=true,  .func=&cmd_selftest_handler,               .description_text=T_CMDLN_SELFTEST,     .category=CMD_CAT_SYSTEM },
{ .command="bug",       .allow_hiz=true,  .func=&bug_handler,                        .def=&bug_def, .description_text=0x00,                 .category=CMD_CAT_SYSTEM },
{ .command="ovrclk",    .allow_hiz=true,  .func=&ovrclk_handler,                     .def=&ovrclk_def, .description_text=0x00,                 .category=CMD_CAT_SYSTEM },
// Files: storage and file operations
{ .command="ls",        .allow_hiz=true,  .func=&disk_ls_handler,   .def=&disk_ls_def, .description_text=T_CMDLN_LS,           .category=CMD_CAT_FILES },
{ .command="cd",        .allow_hiz=true,  .func=&disk_cd_handler,   .def=&disk_cd_def, .description_text=T_CMDLN_CD,           .category=CMD_CAT_FILES },
{ .command="mkdir",     .allow_hiz=true,  .func=&disk_mkdir_handler,.def=&disk_mkdir_def,.description_text=T_CMDLN_MKDIR,        .category=CMD_CAT_FILES },
{ .command="rm",        .allow_hiz=true,  .func=&disk_rm_handler,   .def=&disk_rm_def, .description_text=T_CMDLN_RM,           .category=CMD_CAT_FILES },
{ .command="cat",       .allow_hiz=true,  .func=&disk_cat_handler,  .def=&disk_cat_def,.description_text=T_CMDLN_CAT,          .category=CMD_CAT_FILES },
{ .command="hex",       .allow_hiz=true,  .func=&hex_handler,   .def=&hex_def,     .description_text=T_CMDLN_HEX,          .category=CMD_CAT_FILES },
{ .command="format",    .allow_hiz=true,  .func=&disk_format_handler,.def=&disk_format_def,.description_text=T_HELP_CMD_FORMAT,    .category=CMD_CAT_FILES },
{ .command="label",     .allow_hiz=true,  .func=&disk_label_handler,.def=&disk_label_def,.description_text=T_HELP_CMD_LABEL,     .category=CMD_CAT_FILES },
{ .command="image",     .allow_hiz=true,  .func=&image_handler,     .def=&image_def,     .description_text=T_HELP_CMD_IMAGE,     .category=CMD_CAT_FILES },
{ .command="dump",      .allow_hiz=false, .func=&dump_handler,      .def=&dump_def,      .description_text=T_CMDLN_DUMP,         .category=CMD_CAT_FILES },
#if RPI_PLATFORM == RP2350
{ .command="otpdump",   .allow_hiz=true,  .func=&otpdump_handler,   .def=&otpdump_def,   .description_text=0x00,                 .category=CMD_CAT_FILES },
#endif
// Script: scripting and macros
{ .command="script",    .allow_hiz=true,  .func=&script_handler,    .def=&script_def,    .description_text=T_HELP_CMD_SCRIPT,    .category=CMD_CAT_SCRIPT },
{ .command="button",    .allow_hiz=true,  .func=&button_scr_handler,.def=&button_scr_def,.description_text=T_HELP_CMD_BUTTON,    .category=CMD_CAT_SCRIPT },
{ .command="macro",     .allow_hiz=true,  .func=&macro_handler,     .def=&macro_def,     .description_text=T_HELP_CMD_MACRO,     .category=CMD_CAT_SCRIPT },
{ .command="pause",     .allow_hiz=true,  .func=&pause_handler,     .def=&pause_def,     .description_text=T_HELP_CMD_PAUSE,     .category=CMD_CAT_SCRIPT },
// Tools: utilities and converters
{ .command="logic",     .allow_hiz=true,  .func=&logic_handler,                      .description_text=T_HELP_CMD_LOGIC,     .category=CMD_CAT_TOOLS },
{ .command="smps",      .allow_hiz=true,  .func=&smps_handler,      .def=&smps_def,      .description_text=T_HELP_CMD_SMPS,      .category=CMD_CAT_SYSTEM },
{ .command="=",         .allow_hiz=true,  .func=&cmd_convert_base_handler,           .def=&convert_base_def, .description_text=T_CMDLN_INT_FORMAT,   .category=CMD_CAT_TOOLS },
{ .command="|",         .allow_hiz=true,  .func=&cmd_convert_inverse_handler,        .def=&convert_inverse_def, .description_text=T_CMDLN_INT_INVERSE,  .category=CMD_CAT_TOOLS },
{ .command="jep106",    .allow_hiz=true,  .func=&jep106_handler,    .def=&jep106_def,    .description_text=T_HELP_GLOBAL_JEP106_LOOKUP,.category=CMD_CAT_SYSTEM },
// Mode: mode selection and binmode
{ .command="m",         .allow_hiz=true,  .func=&ui_mode_enable_args,                .description_text=T_CMDLN_MODE,         .category=CMD_CAT_MODE },
{ .command="binmode",   .allow_hiz=true,  .func=&cmd_binmode_handler,.def=&cmd_binmode_def,.description_text=T_CONFIG_BINMODE_SELECT, .category=CMD_CAT_MODE },
// Hidden: aliases and internal commands (not shown in help)
{ .command="?",         .allow_hiz=true,  .func=&help_handler,                       .def=&help_def, .description_text=0x00,                 .category=CMD_CAT_HIDDEN },
{ .command="h",         .allow_hiz=true,  .func=&help_handler,                       .def=&help_def, .description_text=0x00,                 .category=CMD_CAT_HIDDEN },
{ .command="help",      .allow_hiz=true,  .func=&help_handler,                       .def=&help_def, .description_text=T_CMDLN_HELP,         .category=CMD_CAT_HIDDEN },
{ .command="dummy",     .allow_hiz=true,  .func=&dummy_handler,                      .def=&dummy_def, .description_text=0x00,                 .category=CMD_CAT_HIDDEN },
{ .command="flat",      .allow_hiz=true,  .func=&flat_handler,                       .def=&flat_def, .description_text=0x00,                 .category=CMD_CAT_HIDDEN },
// clang-format on
};

const uint32_t commands_count = count_of(commands);
