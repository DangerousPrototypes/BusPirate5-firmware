/**
 * @file syntax_compile.c
 * @brief Syntax compiler - parses command line to bytecode.
 * @details Phase 1 of syntax processing: converts user input into
 *          executable bytecode instructions.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "syntax.h"
#include "syntax_internal.h"
#include "pirate/bio.h"

// #define SYNTAX_DEBUG

/*
 * =============================================================================
 * Global state (shared with syntax_run.c and syntax_post.c)
 * =============================================================================
 */

struct _syntax_io syntax_io = { .out_cnt = 0, .in_cnt = 0 };

const struct _bytecode bytecode_empty;

/*
 * =============================================================================
 * Command symbol to bytecode mapping
 * =============================================================================
 */

const struct _syntax_compile_commands_t syntax_compile_commands[] = {
    {'r', SYN_READ},
    {'[', SYN_START},
    {'{', SYN_START_ALT},
    {']', SYN_STOP},
    {'}', SYN_STOP_ALT},
    {'d', SYN_DELAY_US},
    {'D', SYN_DELAY_MS},
    {'^', SYN_TICK_CLOCK},
    {'/', SYN_SET_CLK_HIGH},
    {'\\', SYN_SET_CLK_LOW},
    {'_', SYN_SET_DAT_LOW},
    {'-', SYN_SET_DAT_HIGH},
    {'.', SYN_READ_DAT},
    {'a', SYN_AUX_OUTPUT_LOW},
    {'A', SYN_AUX_OUTPUT_HIGH},
    {'@', SYN_AUX_INPUT},
    {'v', SYN_ADC}
};

const size_t syntax_compile_commands_count = count_of(syntax_compile_commands);

/*
 * =============================================================================
 * Compiler implementation
 * =============================================================================
 */

SYNTAX_STATUS syntax_compile(void) {
    uint32_t current_position = 0;
    uint32_t generated_in_cnt = 0;
    uint32_t i;
    char c;

    // Reset buffers
    syntax_io.out_cnt = 0;
    syntax_io.in_cnt = 0;
    for (i = 0; i < SYN_MAX_LENGTH; i++) {
        syntax_io.out[i] = bytecode_empty;
        syntax_io.in[i] = bytecode_empty;
    }

    // Track pin functions to detect conflicts
    enum bp_pin_func pin_func[HW_PINS - 2];
    for (i = 1; i < HW_PINS - 1; i++) {
        pin_func[i - 1] = system_config.pin_func[i];
    }

    while (cmdln_try_peek(0, &c)) {
        current_position++;

        // Skip whitespace and special characters
        if (c <= ' ' || c > '~' || c == '>') {
            cmdln_try_discard(1);
            continue;
        }

        // Parse number
        if (c >= '0' && c <= '9') {
            struct prompt_result result;
            ui_parse_get_int(&result, &syntax_io.out[syntax_io.out_cnt].out_data);
            if (result.error) {
                printf("Error parsing integer at position %d\r\n", current_position);
                return SSTATUS_ERROR;
            }
            syntax_io.out[syntax_io.out_cnt].command = SYN_WRITE;
            syntax_io.out[syntax_io.out_cnt].number_format = result.number_format;
            goto compiler_get_attributes;
        }

        // Parse string literal
        if (c == '"') {
            cmdln_try_remove(&c); // consume opening "
            
            // Find terminating "
            i = 0;
            while (cmdln_try_peek(i, &c)) {
                if (c == '"') {
                    if ((syntax_io.out_cnt + i) >= SYN_MAX_LENGTH) {
                        printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
                        return SSTATUS_ERROR;
                    }
                    goto compile_get_string;
                }
                i++;
            }
            printf("Error: string missing terminating '\"'");
            return SSTATUS_ERROR;

compile_get_string:
            while (i--) {
                cmdln_try_remove(&c);
                syntax_io.out[syntax_io.out_cnt].command = SYN_WRITE;
                syntax_io.out[syntax_io.out_cnt].out_data = c;
                syntax_io.out[syntax_io.out_cnt].has_repeat = false;
                syntax_io.out[syntax_io.out_cnt].repeat = 1;
                syntax_io.out[syntax_io.out_cnt].number_format = df_ascii;
                syntax_io.out[syntax_io.out_cnt].bits = system_config.num_bits;
                syntax_io.out_cnt++;
                generated_in_cnt++;
            }
            cmdln_try_remove(&c); // consume closing "
            if (generated_in_cnt >= SYN_MAX_LENGTH) {
                printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
                return SSTATUS_ERROR;
            }
            continue;
        }

        // Look up command symbol
        bool found = false;
        for (i = 0; i < syntax_compile_commands_count; i++) {
            if (c == syntax_compile_commands[i].symbol) {
                syntax_io.out[syntax_io.out_cnt].command = syntax_compile_commands[i].code;
                cmdln_try_discard(1);
                found = true;
                goto compiler_get_attributes;
            }
        }
        
        if (!found) {
            printf("Unknown syntax '%c' at position %d\r\n", c, current_position);
            return SSTATUS_ERROR;
        }

compiler_get_attributes:
        // Parse .bits attribute
        if (ui_parse_get_dot(&syntax_io.out[syntax_io.out_cnt].bits)) {
            syntax_io.out[syntax_io.out_cnt].has_bits = true;
        } else {
            syntax_io.out[syntax_io.out_cnt].has_bits = false;
            syntax_io.out[syntax_io.out_cnt].bits = system_config.num_bits;
        }

        // Parse :repeat attribute
        if (ui_parse_get_colon(&syntax_io.out[syntax_io.out_cnt].repeat)) {
            syntax_io.out[syntax_io.out_cnt].has_repeat = true;
        } else {
            syntax_io.out[syntax_io.out_cnt].has_repeat = false;
            syntax_io.out[syntax_io.out_cnt].repeat = 1;
        }

        // Validate pin commands
        if (syntax_io.out[syntax_io.out_cnt].command >= SYN_AUX_OUTPUT_HIGH) {
            if (!syntax_io.out[syntax_io.out_cnt].has_bits) {
                printf("Error: missing IO number for command %c at position %d. Try %c.0\r\n",
                       c, current_position, c);
                return SSTATUS_ERROR;
            }

            if (syntax_io.out[syntax_io.out_cnt].bits >= count_of(bio2bufiopin)) {
                printf("%sError:%s pin IO%d is invalid\r\n",
                       ui_term_color_error(),
                       ui_term_color_reset(),
                       syntax_io.out[syntax_io.out_cnt].bits);
                return SSTATUS_ERROR;
            }

            if (syntax_io.out[syntax_io.out_cnt].command != SYN_ADC &&
                pin_func[syntax_io.out[syntax_io.out_cnt].bits] != BP_PIN_IO) {
                printf("%sError:%s at position %d IO%d is already in use\r\n",
                       ui_term_color_error(),
                       ui_term_color_reset(),
                       current_position,
                       syntax_io.out[syntax_io.out_cnt].bits);
                return SSTATUS_ERROR;
            }
        }

        // Track slot usage
        if (syntax_io.out[syntax_io.out_cnt].command == SYN_DELAY_US ||
            syntax_io.out[syntax_io.out_cnt].command == SYN_DELAY_MS ||
            syntax_io.out[syntax_io.out_cnt].command == SYN_TICK_CLOCK) {
            generated_in_cnt += 1;
        } else {
            generated_in_cnt += syntax_io.out[syntax_io.out_cnt].repeat;
        }

        if (generated_in_cnt >= SYN_MAX_LENGTH) {
            printf("Syntax exceeds available space (%d slots)\r\n", SYN_MAX_LENGTH);
            return SSTATUS_ERROR;
        }

        if (syntax_io.out_cnt >= SYN_MAX_LENGTH) {
            printf("Syntax output buffer overflow (max %d commands)\r\n", SYN_MAX_LENGTH);
            return SSTATUS_ERROR;
        }

        syntax_io.out_cnt++;
    }

#ifdef SYNTAX_DEBUG
    for (i = 0; i < syntax_io.out_cnt; i++) {
        printf("%d:%d\r\n", syntax_io.out[i].command, syntax_io.out[i].repeat);
    }
#endif

    syntax_io.in_cnt = 0;
    return SSTATUS_OK;
}
