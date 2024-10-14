
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"

void bitorder_msb(void) {
    system_config.bit_order = 0;
}

void bitorder_msb_handler(struct command_result* res) {
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
    bitorder_lsb();
    printf("%s%s:%s %s 0b0000000%s1%s",
           ui_term_color_notice(),
           GET_T(T_MODE_BITORDER),
           ui_term_color_reset(),
           GET_T(T_MODE_BITORDER_LSB),
           ui_term_color_info(),
           ui_term_color_reset());
}