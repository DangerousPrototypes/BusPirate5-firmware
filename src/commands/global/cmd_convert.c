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
#include "ui/ui_cmdln.h"
#include "lib/bp_expr/bp_expr.h"

const char* const base_usage[] = {
    "= <expression>",
    "Convert: = 0x12",
    "Math: = 0xFF & (1<<4)",
    "Ops: + - * / % & | ^ ~ << >>",
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
        printf("= '%c' ", (char)value);
    }
}

void cmd_convert_base_handler(struct command_result* res) {
    uint32_t result = 0;
    
    // Get the expression (everything after "=")
    const char *expr = ln_cmdln_current();
    size_t len = ln_cmdln_remaining();
    
    // Skip leading whitespace
    while (len > 0 && (*expr == ' ' || *expr == '\t')) {
        expr++;
        len--;
    }
    
    if (len == 0) {
        printf("Usage: = <expression>\r\n");
        printf("  Operators: + - * / %% & | ^ ~ << >>\r\n");
        printf("  Numbers: 0xFF 0b1010 255\r\n");
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
    bool has_value = cmdln_args_uint32_by_position(1, &temp);
    lsb_get(&temp, system_config.num_bits, 1);

    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_hex);
    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_dec);
    printf(" %s| ", ui_term_color_reset());
    ui_format_print_number_3(temp, system_config.num_bits, df_bin);
}
