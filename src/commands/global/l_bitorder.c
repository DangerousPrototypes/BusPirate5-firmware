/**
 * @file l_bitorder.c
 * @brief Bit order control commands implementation.
 * @details Implements the l/L commands for controlling bit transfer order:
 *          - L: MSB-first (most significant bit first) 0b10000000
 *          - l: LSB-first (least significant bit first) 0b00000001
 *          
 *          Affects:
 *          - Data transfer in protocol modes
 *          - Number display formatting
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const msb_usage[] = {
    "l",
    "Set bit order to MSB-first:%s l",
};

static const char* const lsb_usage[] = {
    "L",
    "Set bit order to LSB-first:%s L",
};

const bp_command_def_t bitorder_msb_def = {
    .name         = "l",
    .description  = T_CMDLN_BITORDER_MSB,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .usage        = msb_usage,
    .usage_count  = count_of(msb_usage),
};

const bp_command_def_t bitorder_lsb_def = {
    .name         = "L",
    .description  = T_CMDLN_BITORDER_LSB,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .usage        = lsb_usage,
    .usage_count  = count_of(lsb_usage),
};

void bitorder_msb(void) {
    system_config.bit_order = 0;
}

void bitorder_msb_handler(struct command_result* res) {
    if (bp_cmd_help_check(&bitorder_msb_def, res->help_flag)) {
        return;
    }
    bitorder_msb();
    printf("%s%s:%s %s 0b%s1%s0000000",
           ui_term_color_notice(),
           GET_T(T_MODE_BITORDER),
           ui_term_color_reset(),
           GET_T(T_MODE_BITORDER_MSB),
           ui_term_color_info(),
           ui_term_color_reset());
}

void bitorder_lsb(void) {
    system_config.bit_order = 1;
}

void bitorder_lsb_handler(struct command_result* res) {
    if (bp_cmd_help_check(&bitorder_lsb_def, res->help_flag)) {
        return;
    }
    bitorder_lsb();
    printf("%s%s:%s %s 0b0000000%s1%s",
           ui_term_color_notice(),
           GET_T(T_MODE_BITORDER),
           ui_term_color_reset(),
           GET_T(T_MODE_BITORDER_LSB),
           ui_term_color_info(),
           ui_term_color_reset());
}