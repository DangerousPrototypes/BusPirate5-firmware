/**
 * @file bp_args_compat.h
 * @brief Compatibility layer for old cmdln_args_* API using bp_args.
 * @details Provides drop-in replacement functions that use bp_args internally.
 *          Include this instead of modifying existing command handlers.
 *          
 *          OLD API:
 *            cmdln_args_find_flag('v')
 *            cmdln_args_find_flag_uint32('c', &arg, &value)
 *            cmdln_args_uint32_by_position(1, &value)
 *          
 *          NEW API (preferred):
 *            static const bp_arg_opt_t opts[] = {
 *                {"verbose", 'v', BP_ARG_NONE},
 *                {"count",   'c', BP_ARG_REQUIRED},
 *                {0}
 *            };
 *            bp_args_t args;
 *            bp_args_init(&args, ln_cmdln_current(), ln_cmdln_remaining());
 *            while ((opt = bp_args_next(&args, opts)) != -1) { ... }
 */

#ifndef BP_ARGS_COMPAT_H
#define BP_ARGS_COMPAT_H

#include <stdbool.h>
#include <stdint.h>
#include "pirate.h"
#include "lib/bp_args/bp_args.h"
#include "lib/bp_linenoise/ln_cmdreader.h"
#include "ui/ui_cmdln.h"

/**
 * @brief Default options for compatibility (supports any single-char flag).
 * @details Used when commands don't define their own option specs.
 */
extern const bp_arg_opt_t bp_args_compat_any_flag[];

/**
 * @brief Initialize compatibility parser for current command.
 * @return Initialized bp_args_t structure
 */
static inline bp_args_t bp_args_compat_init(void) {
    bp_args_t args;
    bp_args_init(&args, ln_cmdln_current(), ln_cmdln_remaining());
    return args;
}

/*
 * =============================================================================
 * Compatibility wrappers for old API
 * =============================================================================
 */

/**
 * @brief Check if a flag is present (compat wrapper).
 * @param flag  Flag character
 * @return true if flag found
 * @note Replacement for: cmdln_args_find_flag(char flag)
 */
bool bp_compat_find_flag(char flag);

/**
 * @brief Find flag with uint32 value (compat wrapper).
 * @param flag    Flag character
 * @param arg     Output arg info (for backward compat, can be NULL)
 * @param value   Output value
 * @return true if flag found with value
 * @note Replacement for: cmdln_args_find_flag_uint32(char, command_var_t*, uint32_t*)
 */
bool bp_compat_find_flag_uint32(char flag, command_var_t* arg, uint32_t* value);

/**
 * @brief Find flag with string value (compat wrapper).
 * @param flag    Flag character
 * @param arg     Output arg info (can be NULL)
 * @param max_len Maximum string length
 * @param str     Output string buffer
 * @return true if flag found with value
 * @note Replacement for: cmdln_args_find_flag_string(char, command_var_t*, uint32_t, char*)
 */
bool bp_compat_find_flag_string(char flag, command_var_t* arg, uint32_t max_len, char* str);

/**
 * @brief Get positional uint32 argument (compat wrapper).
 * @param pos    Position (0 = first after command, 1 = second, etc.)
 * @param value  Output value
 * @return true if found
 * @note Replacement for: cmdln_args_uint32_by_position(uint32_t, uint32_t*)
 */
bool bp_compat_uint32_by_position(uint32_t pos, uint32_t* value);

/**
 * @brief Get positional float argument (compat wrapper).
 * @param pos    Position
 * @param value  Output value
 * @return true if found
 * @note Replacement for: cmdln_args_float_by_position(uint32_t, float*)
 */
bool bp_compat_float_by_position(uint32_t pos, float* value);

/**
 * @brief Get positional string argument (compat wrapper).
 * @param pos      Position
 * @param max_len  Maximum string length
 * @param str      Output buffer
 * @return true if found
 * @note Replacement for: cmdln_args_string_by_position(uint32_t, uint32_t, char*)
 */
bool bp_compat_string_by_position(uint32_t pos, uint32_t max_len, char* str);

/*
 * =============================================================================
 * Macro redirects (drop-in replacement)
 * =============================================================================
 */

// Uncomment these to redirect old API to new implementation:
// #define cmdln_args_find_flag          bp_compat_find_flag
// #define cmdln_args_find_flag_uint32   bp_compat_find_flag_uint32
// #define cmdln_args_find_flag_string   bp_compat_find_flag_string
// #define cmdln_args_uint32_by_position bp_compat_uint32_by_position
// #define cmdln_args_float_by_position  bp_compat_float_by_position
// #define cmdln_args_string_by_position bp_compat_string_by_position

#endif // BP_ARGS_COMPAT_H
