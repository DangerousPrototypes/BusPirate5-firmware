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
#include "commands/global/menu_demo.h"
#include "commands/global/txtest.h"
#include "commands/global/disk.h"
#include "commands/global/i_info.h"
#include "commands/global/macro.h"
#include "commands/global/script.h"
#include "commands/global/edit.h"
#include "commands/global/hexedit.h"
#include "commands/global/button_scr.h"
#include "commands/global/smps.h"
#include "commands/global/cls.h"
#include "commands/global/logic.h"
#include "commands/global/cmd_toolbar.h"
#include "commands/global/cmd_binmode.h"
#include "commands/global/bug.h"
#include "commands/global/image.h"
#include "commands/global/dump.h"
#include "commands/global/hex.h"
#include "commands/global/jep106_lookup.h"
#include "commands/global/life.h"
#include "commands/global/snake.h"
#include "commands/global/game2048.h"
#include "commands/global/tictactoe.h"
#include "commands/global/hangman.h"
#include "commands/global/mines.h"
#include "commands/global/bricks.h"
#include "commands/global/drop4.h"
#include "commands/global/fleet.h"
#include "commands/global/worm.h"
#include "commands/global/pcbrun.h"
#include "commands/global/invaders.h"
#include "commands/global/sigride.h"
#include "commands/global/stkover.h"
#include "commands/global/cryptocrack.h"
#include "commands/global/wiretrace.h"
#include "commands/global/logicgates.h"
#include "commands/global/rogueprobe.h"
#include "commands/global/crossflash.h"
#if RPI_PLATFORM == RP2350
#include "commands/global/otpdump.h"
#endif
#include "commands/global/ovrclk.h"

// command configuration
const struct _global_command_struct commands[] = {
    // clang-format off
// IO: pin control, power, measurement
{ .command="W",         .allow_hiz=false, .func=&psucmd_enable_handler,              .def=&psucmd_enable_def, .category=CMD_CAT_IO },
{ .command="w",         .allow_hiz=false, .func=&psucmd_disable_handler,             .def=&psucmd_disable_def, .category=CMD_CAT_IO },
{ .command="P",         .allow_hiz=false, .func=&pullups_enable_handler,             .def=&pullups_enable_def, .category=CMD_CAT_IO },
{ .command="p",         .allow_hiz=false, .func=&pullups_disable_handler,            .def=&pullups_disable_def, .category=CMD_CAT_IO },
{ .command="a",         .allow_hiz=false, .func=&auxio_low_handler,                  .def=&auxio_low_def, .category=CMD_CAT_IO },
{ .command="A",         .allow_hiz=false, .func=&auxio_high_handler,                 .def=&auxio_high_def, .category=CMD_CAT_IO },
{ .command="@",         .allow_hiz=true,  .func=&auxio_input_handler,                .def=&auxio_input_def, .category=CMD_CAT_IO },
{ .command="f",         .allow_hiz=true,  .func=&freq_single,                        .def=&freq_single_def, .category=CMD_CAT_IO },
{ .command="F",         .allow_hiz=true,  .func=&freq_cont,                          .def=&freq_cont_def, .category=CMD_CAT_IO },
{ .command="G",         .allow_hiz=false, .func=&pwm_configure_enable,               .def=&pwm_enable_def, .category=CMD_CAT_IO },
{ .command="g",         .allow_hiz=false, .func=&pwm_configure_disable,              .def=&pwm_disable_def, .category=CMD_CAT_IO },
{ .command="v",         .allow_hiz=true,  .func=&adc_measure_single,                 .def=&adc_single_def, .category=CMD_CAT_IO },
{ .command="V",         .allow_hiz=true,  .func=&adc_measure_cont,                   .def=&adc_cont_def, .category=CMD_CAT_IO },
// Configure: terminal, display, mode config
{ .command="c",         .allow_hiz=true,  .func=&ui_config_main_menu,                .def=&ui_config_def, .category=CMD_CAT_CONFIGURE },
{ .command="d",         .allow_hiz=true,  .func=&ui_display_enable_args,             .def=&display_select_def, .category=CMD_CAT_CONFIGURE },
{ .command="o",         .allow_hiz=true,  .func=&ui_mode_int_display_format,         .def=&display_format_def, .category=CMD_CAT_CONFIGURE },
{ .command="l",         .allow_hiz=true,  .func=&bitorder_msb_handler,               .def=&bitorder_msb_def, .category=CMD_CAT_CONFIGURE },
{ .command="L",         .allow_hiz=true,  .func=&bitorder_lsb_handler,               .def=&bitorder_lsb_def, .category=CMD_CAT_CONFIGURE },
{ .command="cls",       .allow_hiz=true,  .func=&ui_display_clear,                   .def=&cls_def, .category=CMD_CAT_CONFIGURE },
// System: info, reboot, selftest
{ .command="i",         .allow_hiz=true,  .func=&i_info_handler,                     .def=&i_info_def, .category=CMD_CAT_SYSTEM },
{ .command="reboot",    .allow_hiz=true,  .func=&cmd_mcu_reboot_handler,             .def=&reboot_def, .category=CMD_CAT_SYSTEM },
{ .command="$",         .allow_hiz=true,  .func=&cmd_mcu_jump_to_bootloader_handler, .def=&bootloader_def, .category=CMD_CAT_SYSTEM },
{ .command="~",         .allow_hiz=true,  .func=&cmd_selftest_handler,               .def=&cmd_selftest_def, .category=CMD_CAT_SYSTEM },
{ .command="bug",       .allow_hiz=true,  .func=&bug_handler,                        .def=&bug_def, .category=CMD_CAT_SYSTEM },
{ .command="ovrclk",    .allow_hiz=true,  .func=&ovrclk_handler,                     .def=&ovrclk_def, .category=CMD_CAT_SYSTEM },
// Files: storage and file operations
{ .command="ls",        .allow_hiz=true,  .func=&disk_ls_handler,   .def=&disk_ls_def, .category=CMD_CAT_FILES },
{ .command="cd",        .allow_hiz=true,  .func=&disk_cd_handler,   .def=&disk_cd_def, .category=CMD_CAT_FILES },
{ .command="mkdir",     .allow_hiz=true,  .func=&disk_mkdir_handler,.def=&disk_mkdir_def,.category=CMD_CAT_FILES },
{ .command="rm",        .allow_hiz=true,  .func=&disk_rm_handler,   .def=&disk_rm_def, .category=CMD_CAT_FILES },
{ .command="cat",       .allow_hiz=true,  .func=&disk_cat_handler,  .def=&disk_cat_def,.category=CMD_CAT_FILES },
{ .command="hex",       .allow_hiz=true,  .func=&hex_handler,   .def=&hex_def,     .category=CMD_CAT_FILES },
{ .command="format",    .allow_hiz=true,  .func=&disk_format_handler,.def=&disk_format_def,.category=CMD_CAT_FILES },
{ .command="label",     .allow_hiz=true,  .func=&disk_label_handler,.def=&disk_label_def,.category=CMD_CAT_FILES },
{ .command="image",     .allow_hiz=true,  .func=&image_handler,     .def=&image_def,     .category=CMD_CAT_FILES },
{ .command="dump",      .allow_hiz=false, .func=&dump_handler,      .def=&dump_def,      .category=CMD_CAT_FILES },
#if RPI_PLATFORM == RP2350
{ .command="otpdump",   .allow_hiz=true,  .func=&otpdump_handler,   .def=&otpdump_def,   .category=CMD_CAT_FILES },
#endif
// Script: scripting and macros
{ .command="script",    .allow_hiz=true,  .func=&script_handler,    .def=&script_def,    .category=CMD_CAT_SCRIPT },
{ .command="button",    .allow_hiz=true,  .func=&button_scr_handler,.def=&button_scr_def,.category=CMD_CAT_SCRIPT },
{ .command="macro",     .allow_hiz=true,  .func=&macro_handler,     .def=&macro_def,     .category=CMD_CAT_SCRIPT },
{ .command="pause",     .allow_hiz=true,  .func=&pause_handler,     .def=&pause_def,     .category=CMD_CAT_SCRIPT },
{ .command="edit",      .allow_hiz=true,  .func=&edit_handler,      .def=&edit_def,      .category=CMD_CAT_SCRIPT },
{ .command="hexedit",   .allow_hiz=true,  .func=&hexedit_handler,   .def=&hexedit_def,   .category=CMD_CAT_SCRIPT },
// Tools: utilities and converters
{ .command="logic",     .allow_hiz=true,  .func=&logic_handler,                      .def=&logic_def, .category=CMD_CAT_TOOLS },
{ .command="toolbar",   .allow_hiz=true,  .func=&toolbar_cmd_handler,                .def=&toolbar_cmd_def, .category=CMD_CAT_TOOLS },
{ .command="life",      .allow_hiz=true,  .func=&life_handler,      .def=&life_def,      .category=CMD_CAT_TOOLS },
{ .command="snake",     .allow_hiz=true,  .func=&snake_handler,     .def=&snake_def,     .category=CMD_CAT_TOOLS },
{ .command="2048",      .allow_hiz=true,  .func=&game2048_handler,  .def=&game2048_def,  .category=CMD_CAT_TOOLS },
{ .command="ttt",       .allow_hiz=true,  .func=&tictactoe_handler, .def=&tictactoe_def, .category=CMD_CAT_TOOLS },
{ .command="hangman",   .allow_hiz=true,  .func=&hangman_handler,   .def=&hangman_def,   .category=CMD_CAT_TOOLS },
{ .command="mines",     .allow_hiz=true,  .func=&mines_handler,     .def=&mines_def,     .category=CMD_CAT_TOOLS },
{ .command="bricks",    .allow_hiz=true,  .func=&bricks_handler,    .def=&bricks_def,    .category=CMD_CAT_TOOLS },
{ .command="drop4",     .allow_hiz=true,  .func=&drop4_handler,     .def=&drop4_def,     .category=CMD_CAT_TOOLS },
{ .command="fleet",     .allow_hiz=true,  .func=&fleet_handler,     .def=&fleet_def,     .category=CMD_CAT_TOOLS },
{ .command="worm",      .allow_hiz=true,  .func=&worm_handler,      .def=&worm_def,      .category=CMD_CAT_TOOLS },
{ .command="pcbrun",    .allow_hiz=true,  .func=&pcbrun_handler,    .def=&pcbrun_def,    .category=CMD_CAT_TOOLS },
{ .command="invaders",  .allow_hiz=true,  .func=&invaders_handler,  .def=&invaders_def,  .category=CMD_CAT_TOOLS },
{ .command="sigride",   .allow_hiz=true,  .func=&sigride_handler,   .def=&sigride_def,   .category=CMD_CAT_TOOLS },
{ .command="stkover",   .allow_hiz=true,  .func=&stkover_handler,   .def=&stkover_def,   .category=CMD_CAT_TOOLS },
{ .command="xflash",   .allow_hiz=true,  .func=&crossflash_handler,.def=&crossflash_def,.category=CMD_CAT_TOOLS },
{ .command="crack",    .allow_hiz=true,  .func=&cryptocrack_handler,.def=&cryptocrack_def,.category=CMD_CAT_TOOLS },
{ .command="trace",    .allow_hiz=true,  .func=&wiretrace_handler, .def=&wiretrace_def, .category=CMD_CAT_TOOLS },
{ .command="gates",    .allow_hiz=true,  .func=&logicgates_handler,.def=&logicgates_def,.category=CMD_CAT_TOOLS },
{ .command="rogue",   .allow_hiz=true,  .func=&rogueprobe_handler,.def=&rogueprobe_def,.category=CMD_CAT_TOOLS },
{ .command="smps",      .allow_hiz=true,  .func=&smps_handler,      .def=&smps_def,      .category=CMD_CAT_SYSTEM },
{ .command="=",         .allow_hiz=true,  .func=&cmd_convert_base_handler,           .def=&convert_base_def, .category=CMD_CAT_TOOLS },
{ .command="|",         .allow_hiz=true,  .func=&cmd_convert_inverse_handler,        .def=&convert_inverse_def, .category=CMD_CAT_TOOLS },
{ .command="jep106",    .allow_hiz=true,  .func=&jep106_handler,    .def=&jep106_def,    .category=CMD_CAT_SYSTEM },
// Mode: mode selection and binmode
{ .command="m",         .allow_hiz=true,  .func=&ui_mode_enable_args, .def=&mode_def,  .category=CMD_CAT_MODE },
{ .command="binmode",   .allow_hiz=true,  .func=&cmd_binmode_handler,.def=&cmd_binmode_def,.category=CMD_CAT_MODE },
// Hidden: aliases and internal commands (not shown in help)
{ .command="?",         .allow_hiz=true,  .func=&help_handler,                       .def=&help_def, .category=CMD_CAT_HIDDEN },
{ .command="h",         .allow_hiz=true,  .func=&help_handler,                       .def=&help_def, .category=CMD_CAT_HIDDEN },
{ .command="help",      .allow_hiz=true,  .func=&help_handler,                       .def=&help_def, .category=CMD_CAT_HIDDEN },
{ .command="dummy",     .allow_hiz=true,  .func=&dummy_handler,                      .def=&dummy_def, .category=CMD_CAT_HIDDEN },
{ .command="menu_demo", .allow_hiz=true,  .func=&menu_demo_handler,                  .def=&menu_demo_def, .category=CMD_CAT_HIDDEN },
{ .command="txtest",    .allow_hiz=true,  .func=&txtest_handler,                     .def=&txtest_def, .category=CMD_CAT_HIDDEN },
// clang-format on
};

const uint32_t commands_count = count_of(commands);
