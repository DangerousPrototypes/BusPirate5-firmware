#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "command_attributes.h" //fix ui_format and get rid of this only struct...
#include "ui/ui_format.h"
#include "pirate/lsb.h"
#include "ui/ui_const.h"
#include "ui/ui_cmdln.h"

const char* const base_usage[] = {
    "= <value>",
    "Convert HEX: = 0x12",
    "Convert DEC: = 18",
    "Convert BIN: = 0b10010",
};

const struct ui_help_options base_options[] = {
    { 1, "", T_HELP_GCMD_P }, // command help
    { 0, "p", T_CONFIG_DISABLE },
    { 0, "P", T_CONFIG_ENABLE },
};

void cmd_convert_base(uint32_t value, uint32_t num_bits) {
    printf(" %s=", ui_term_color_reset());
    ui_format_print_number_3(value, num_bits, df_hex);
    printf(" %s=", ui_term_color_reset());
    ui_format_print_number_3(value, num_bits, df_dec);
    printf(" %s=", ui_term_color_reset());
    ui_format_print_number_3(value, num_bits, df_bin);
    if (value >= ' ' && value <= '~') {
        printf("= '%c' ", value);
    }
}

void cmd_convert_base_handler(struct command_result* res) {
    uint32_t temp = 0, num_bits = 32;
    bool has_value = cmdln_args_uint32_by_position(1, &temp);
    uint32_t mask = 0xff000000;
    for (uint8_t i = 0; i < 3; i++) { // 4 = 32 bit support TODO: this really isn't doing what we need...
        if (!(temp & (mask >> (i * 8)))) {
            num_bits -= 8;
        }
    }
    cmd_convert_base(temp, num_bits);
}

void cmd_convert_inverse_handler(struct command_result* res) {
    uint32_t temp = 0;
    bool has_value = cmdln_args_uint32_by_position(1, &temp);
    lsb_get(&temp, system_config.num_bits, 1);

    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_hex);
    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_dec);
    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_bin);
}
