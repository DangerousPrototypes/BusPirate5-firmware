/**
 * @file syntax_post.c
 * @brief Syntax post-processor - formats and displays results.
 * @details Phase 3 of syntax processing: formats execution results
 *          for user display with proper number formatting.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_format.h"
#include "syntax.h"
#include "syntax_internal.h"

/*
 * =============================================================================
 * Number formatting helpers (refactored to use ui_format)
 * =============================================================================
 */

/**
 * @brief Print ASCII representation if printable.
 * @param value  Character value to check
 */
static inline void print_ascii_if_printable(uint32_t value) {
    if ((char)value >= ' ' && (char)value <= '~') {
        printf("'%c' ", (char)value);
    } else {
        printf("''  ");
    }
}

/**
 * @brief Format and print a number from bytecode.
 * @param in     Bytecode with formatting info
 * @param value  Value to print
 * @param read   true if this is a read operation
 * 
 * @details Uses ui_format_print_number_3() for consistent formatting.
 *          Handles display format selection based on context.
 */
static void postprocess_format_print_number(struct _bytecode *in, uint32_t *value, bool read) {
    uint32_t d = *value;
    uint8_t num_bits = in->bits;
    uint8_t display_format;

    // Determine display format based on context
    if (!read && (system_config.display_format == df_auto ||
                  system_config.display_format == df_ascii ||
                  in->number_format == df_ascii)) {
        display_format = in->number_format;
    } else {
        display_format = system_config.display_format;
    }

    // Mask value to bit width
    uint32_t mask = (num_bits < 32) ? ((1u << num_bits) - 1) : 0xFFFFFFFF;
    d &= mask;

    // Print ASCII prefix if in ASCII mode
    if (display_format == df_ascii) {
        print_ascii_if_printable(d);
    }

    // Use unified number formatter (handles hex/dec/bin with color)
    ui_format_print_number_3(d, num_bits, 
        (display_format == df_ascii || display_format == df_auto) ? df_hex : display_format);
}

/*
 * =============================================================================
 * Write/Read post-processing
 * =============================================================================
 */

void postprocess_mode_write(struct _bytecode *in, struct _output_info *info) {
    uint32_t repeat;
    uint32_t value;
    uint8_t row_length;
    bool new_line = false;

    // Determine numbers per row based on format
    row_length = 8;
    if (in->number_format == df_bin || system_config.display_format == df_ascii) {
        row_length = 4;
    }

    // Start new row if format or command changed
    if (in->number_format != info->previous_number_format ||
        in->command != info->previous_command) {
        new_line = true;
        info->row_counter = info->row_length = row_length;
        info->previous_number_format = in->number_format;
    }

    if (in->command == SYN_WRITE) {
        value = in->out_data;
        repeat = 1;
        if (new_line) {
            printf("\r\n%sTX:%s ", ui_term_color_info(), ui_term_color_reset());
        }
    }

    if (in->command == SYN_READ) {
        repeat = 1;
        value = in->in_data;
        if (new_line) {
            printf("\r\n%sRX:%s ", ui_term_color_info(), ui_term_color_reset());
        }
    }

    while (repeat--) {
        postprocess_format_print_number(in, &value, (in->command == SYN_READ));
        
        if (in->read_with_write) {
            printf("(");
            postprocess_format_print_number(in, &in->in_data, false);
            printf(")");
        }

        info->row_counter--;
        if (in->data_message) {
            printf(" %s ", in->data_message);
        } else {
            printf(" ");
        }

        if (!info->row_counter) {
            printf("\r\n    ");
            info->row_counter = row_length;
        }
    }
}

/*
 * =============================================================================
 * Post-process handlers for each bytecode instruction
 * =============================================================================
 */

static void syntax_post_write(struct _bytecode *in, struct _output_info *info) {
    postprocess_mode_write(in, info);
}

static void syntax_post_read(struct _bytecode *in, struct _output_info *info) {
    postprocess_mode_write(in, info);
}

static void syntax_post_delay_us_ms(struct _bytecode *in, struct _output_info *info) {
    printf("\r\n%s%s:%s %s%d%s%s",
           ui_term_color_notice(),
           GET_T(T_MODE_DELAY),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           in->repeat,
           ui_term_color_reset(),
           (in->command == SYN_DELAY_US ? GET_T(T_MODE_US) : GET_T(T_MODE_MS)));
}

static void syntax_post_start_stop(struct _bytecode *in, struct _output_info *info) {
    if (in->data_message) {
        printf("\r\n%s", in->data_message);
    }
}

static inline void _syntax_post_aux_output(uint8_t bio, bool direction) {
    printf("\r\nIO%s%d%s set to%s OUTPUT: %s%d%s",
           ui_term_color_num_float(),
           bio,
           ui_term_color_notice(),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           direction,
           ui_term_color_reset());
}

static void syntax_post_aux_output_high(struct _bytecode *in, struct _output_info *info) {
    _syntax_post_aux_output(in->bits, 1);
}

static void syntax_post_aux_output_low(struct _bytecode *in, struct _output_info *info) {
    _syntax_post_aux_output(in->bits, 0);
}

static void syntax_post_aux_input(struct _bytecode *in, struct _output_info *info) {
    printf("\r\nIO%s%d%s set to%s INPUT: %s%d%s",
           ui_term_color_num_float(),
           in->bits,
           ui_term_color_notice(),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           in->in_data,
           ui_term_color_reset());
}

static void syntax_post_adc(struct _bytecode *in, struct _output_info *info) {
    uint32_t mv = (6600 * in->in_data) / 4096;
    printf("\r\n%s%s IO%d:%s %s%d.%d%sV",
           ui_term_color_info(),
           GET_T(T_MODE_ADC_VOLTAGE),
           in->bits,
           ui_term_color_reset(),
           ui_term_color_num_float(),
           mv / 1000,
           (mv % 1000) / 100,
           ui_term_color_reset());
}

static void syntax_post_tick_clock(struct _bytecode *in, struct _output_info *info) {
    printf("\r\n%s%s:%s %s%d%s",
           ui_term_color_notice(),
           GET_T(T_MODE_TICK_CLOCK),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           in->repeat,
           ui_term_color_reset());
}

static void syntax_post_set_clk_high_low(struct _bytecode *in, struct _output_info *info) {
    printf("\r\n%s%s:%s %s%d%s",
           ui_term_color_notice(),
           GET_T(T_MODE_SET_CLK),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           in->out_data,
           ui_term_color_reset());
}

static void syntax_post_set_dat_high_low(struct _bytecode *in, struct _output_info *info) {
    printf("\r\n%s%s:%s %s%d%s",
           ui_term_color_notice(),
           GET_T(T_MODE_SET_DAT),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           in->out_data,
           ui_term_color_reset());
}

static void syntax_post_read_dat(struct _bytecode *in, struct _output_info *info) {
    printf("\r\n%s%s:%s %s%d%s",
           ui_term_color_notice(),
           GET_T(T_MODE_READ_DAT),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           in->in_data,
           ui_term_color_reset());
}

/*
 * =============================================================================
 * Dispatch table
 * =============================================================================
 */

syntax_post_func_ptr_t syntax_post_func[] = {
    [SYN_WRITE]          = syntax_post_write,
    [SYN_READ]           = syntax_post_read,
    [SYN_START]          = syntax_post_start_stop,
    [SYN_START_ALT]      = syntax_post_start_stop,
    [SYN_STOP]           = syntax_post_start_stop,
    [SYN_STOP_ALT]       = syntax_post_start_stop,
    [SYN_DELAY_US]       = syntax_post_delay_us_ms,
    [SYN_DELAY_MS]       = syntax_post_delay_us_ms,
    [SYN_AUX_OUTPUT_HIGH]= syntax_post_aux_output_high,
    [SYN_AUX_OUTPUT_LOW] = syntax_post_aux_output_low,
    [SYN_AUX_INPUT]      = syntax_post_aux_input,
    [SYN_ADC]            = syntax_post_adc,
    [SYN_TICK_CLOCK]     = syntax_post_tick_clock,
    [SYN_SET_CLK_HIGH]   = syntax_post_set_clk_high_low,
    [SYN_SET_CLK_LOW]    = syntax_post_set_clk_high_low,
    [SYN_SET_DAT_HIGH]   = syntax_post_set_dat_high_low,
    [SYN_SET_DAT_LOW]    = syntax_post_set_dat_high_low,
    [SYN_READ_DAT]       = syntax_post_read_dat
};

/*
 * =============================================================================
 * Main post-process function
 * =============================================================================
 */

SYNTAX_STATUS syntax_post(void) {
    static struct _output_info info;

    if (!syntax_io.in_cnt) {
        return SSTATUS_ERROR;
    }

    // Reset state for new output
    info.previous_command = 0xFF;

    for (uint32_t pos = 0; pos < syntax_io.in_cnt; pos++) {
        if (syntax_io.in[pos].command >= count_of(syntax_post_func)) {
            printf("Unknown internal code %d\r\n", syntax_io.in[pos].command);
            continue;
        }

        syntax_post_func[syntax_io.in[pos].command](&syntax_io.in[pos], &info);
        info.previous_command = syntax_io.in[pos].command;

        if (syntax_io.in[pos].error) {
            printf("(%s) ", syntax_io.in[pos].error_message);
        }
    }

    printf("\r\n");
    syntax_io.in_cnt = 0;
    return SSTATUS_OK;
}
