#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "ui/ui_format.h"
#include "pirate/lsb.h"
#include "ui/ui_const.h"
#include "lib/bp_args/bp_cmd.h"
#include "lib/bp_expr/bp_expr.h"

const char* const base_usage[] = {
    "= <expression>",
    "Convert: = 0x12",
    "Math: = 0xFF & (1<<4)",
    "Ops: + - * / % & | ^ ~ << >>",
};

const bp_command_def_t convert_base_def = {
    .name = "=",
    .description = T_CMDLN_INT_FORMAT,
    .usage = base_usage,
    .usage_count = count_of(base_usage)
};

const char *const inverse_usage[] = {
    "| <value>",
    "Inverse bits: | 0x12345678",
};

const bp_command_def_t convert_inverse_def = {
    .name = "|",
    .description = T_CMDLN_INT_INVERSE,
    .usage = inverse_usage,
    .usage_count = count_of(inverse_usage)
};

void cmd_convert_base(uint32_t value, uint32_t num_bits) {
    printf(" %s=", ui_term_color_reset());
    ui_format_print_number_3(value, num_bits, df_hex);
    printf(" %s=", ui_term_color_reset());
    ui_format_print_number_3(value, num_bits, df_dec);
    printf(" %s=", ui_term_color_reset());
    ui_format_print_number_3(value, num_bits, df_bin);
    if (value >= ' ' && value <= '~') {
        printf("= '%c' ", (char)value);
    }
}

void cmd_convert_base_handler(struct command_result* res) {
    uint32_t result = 0;
    
    const char *expr;
    size_t len;

    if(bp_cmd_help_check(&convert_base_def, res->help_flag)) {
        return;
    }

    if (!bp_cmd_get_remainder(&convert_base_def, &expr, &len)) {
        bp_cmd_help_show(&convert_base_def);
        return;
    }
    
    // Evaluate expression
    bp_expr_err_t err;
    if (!bp_expr_eval_n(expr, len, &result, &err)) {
        printf("%sError:%s %s\r\n", ui_term_color_error(), ui_term_color_reset(), 
               bp_expr_strerror(err));
        return;
    }
    
    // Calculate display bits (round up to nearest 8)
    uint32_t num_bits = 8;
    if (result > 0xFF) num_bits = 16;
    if (result > 0xFFFF) num_bits = 24;
    if (result > 0xFFFFFF) num_bits = 32;
    
    cmd_convert_base(result, num_bits);
}

void cmd_convert_inverse_handler(struct command_result* res) {
    uint32_t temp = 0;

    if(bp_cmd_help_check(&convert_inverse_def, res->help_flag)) {
        return;
    }

    bool has_value = bp_cmd_get_positional_uint32(&convert_inverse_def, 1, &temp);

    if (!has_value) {
        bp_cmd_help_show(&convert_inverse_def);
        return;
    }

    lsb_get(&temp, system_config.num_bits, 1);

    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_hex);
    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_dec);
    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_bin);
}
