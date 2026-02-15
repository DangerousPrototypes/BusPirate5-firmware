/**
 * @file bp_args.h
 * @brief Command line argument parsing for Bus Pirate.
 * @details Zero-allocation argument parser supporting:
 *          - Short options: -f, -v
 *          - Long options: --file, --verbose
 *          - Required and optional arguments: -f value, --file=value
 *          - Positional arguments
 *          - Number formats via bp_number (0x hex, 0b bin, decimal)
 *          
 *          Works directly on linear command buffer (no argv tokenization).
 *          
 * @example
 *     static const bp_arg_opt_t opts[] = {
 *         {"verbose", 'v', BP_ARG_NONE},
 *         {"file",    'f', BP_ARG_REQUIRED},
 *         {"count",   'c', BP_ARG_OPTIONAL},
 *         {0}
 *     };
 *     
 *     bp_args_t args;
 *     bp_args_init(&args, bp_cmdln_current(), bp_cmdln_remaining());
 *     
 *     int opt;
 *     while ((opt = bp_args_next(&args, opts)) != -1) {
 *         switch (opt) {
 *             case 'v': verbose = true; break;
 *             case 'f': bp_args_get_string(&args, filename, sizeof(filename)); break;
 *             case '?': printf("Error: %s\r\n", args.errmsg); return;
 *         }
 *     }
 */

#ifndef BP_ARGS_H
#define BP_ARGS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Option argument requirement.
 */
typedef enum {
    BP_ARG_NONE = 0,      ///< Boolean flag, no argument
    BP_ARG_REQUIRED = 1,  ///< Requires an argument value
    BP_ARG_OPTIONAL = 2   ///< Argument value is optional
} bp_arg_type_t;

/**
 * @brief Option specification.
 * @details Define options as a static const array, terminated by {0}.
 */
typedef struct {
    const char *long_name;   ///< Long option name (without --), NULL if none
    char short_name;         ///< Short option character, 0 if none
    bp_arg_type_t arg_type;  ///< Argument requirement
} bp_arg_opt_t;

/**
 * @brief Parser state structure.
 * @details Initialize with bp_args_init(), then call bp_args_next() in a loop.
 */
typedef struct {
    const char *str;         ///< Current position in buffer
    const char *start;       ///< Start of command arguments (after command name)
    const char *end;         ///< End of buffer
    const char *optarg;      ///< Current option's argument (NULL if none)
    size_t optarg_len;       ///< Length of optarg (not null-terminated!)
    int optopt;              ///< Current option (short name or '?')
    const char *errmsg;      ///< Error message (static string)
    bool done_options;       ///< Set when -- encountered or non-option found
} bp_args_t;

/**
 * @brief Initialize the argument parser.
 * @param args   Parser state to initialize
 * @param cmdline  Pointer to command line buffer (from bp_cmdln_current())
 * @param len      Length of buffer (from bp_cmdln_remaining())
 * @note The first token is skipped (assumed to be the command name).
 */
void bp_args_init(bp_args_t *args, const char *cmdline, size_t len);

/**
 * @brief Parse the next option.
 * @param args  Parser state
 * @param opts  Array of option specifications (terminated by {0})
 * @return Option short_name on success, -1 when done with options, '?' on error
 * @note After this returns -1, use bp_args_positional() to get remaining args.
 */
int bp_args_next(bp_args_t *args, const bp_arg_opt_t *opts);

/**
 * @brief Get the next positional argument.
 * @param args  Parser state
 * @param arg   Output: pointer to argument (not null-terminated!)
 * @param len   Output: length of argument
 * @return true if argument found, false if no more arguments
 * @note Call this after bp_args_next() returns -1.
 */
bool bp_args_positional(bp_args_t *args, const char **arg, size_t *len);

/**
 * @brief Parse current optarg as uint32 (supports 0x, 0b, decimal).
 * @param args   Parser state (uses args->optarg)
 * @param value  Output value
 * @return true on success, false if no optarg or parse error
 */
bool bp_args_get_uint32(bp_args_t *args, uint32_t *value);

/**
 * @brief Parse current optarg as int32 (supports 0x, 0b, decimal, negative).
 * @param args   Parser state
 * @param value  Output value
 * @return true on success
 */
bool bp_args_get_int32(bp_args_t *args, int32_t *value);

/**
 * @brief Parse current optarg as float.
 * @param args   Parser state
 * @param value  Output value
 * @return true on success
 */
bool bp_args_get_float(bp_args_t *args, float *value);

/**
 * @brief Copy current optarg to a string buffer.
 * @param args    Parser state
 * @param buf     Output buffer
 * @param maxlen  Buffer size (including null terminator)
 * @return true on success, false if no optarg or truncated
 */
bool bp_args_get_string(bp_args_t *args, char *buf, size_t maxlen);

/**
 * @brief Check if a flag option is present (convenience function).
 * @param args  Parser state (should be freshly initialized)
 * @param opts  Option specifications
 * @param flag  Short name of flag to find
 * @return true if flag is present
 * @note This scans the entire argument list; use bp_args_next() for efficiency.
 */
bool bp_args_has_flag(bp_args_t *args, const bp_arg_opt_t *opts, char flag);

/**
 * @brief Find a flag and get its uint32 value (convenience function).
 * @param args   Parser state (should be freshly initialized)
 * @param opts   Option specifications
 * @param flag   Short name of option
 * @param value  Output value
 * @return true if option found with valid uint32 value
 */
bool bp_args_find_uint32(bp_args_t *args, const bp_arg_opt_t *opts, char flag, uint32_t *value);

/**
 * @brief Find a flag and get its string value (convenience function).
 * @param args    Parser state (should be freshly initialized)
 * @param opts    Option specifications
 * @param flag    Short name of option
 * @param buf     Output buffer
 * @param maxlen  Buffer size
 * @return true if option found with string value
 */
bool bp_args_find_string(bp_args_t *args, const bp_arg_opt_t *opts, char flag, char *buf, size_t maxlen);

/**
 * @brief Get positional argument by index (convenience function).
 * @param args   Parser state (should be freshly initialized)
 * @param opts   Option specifications (to skip options)
 * @param pos    Position index (0 = first positional arg after command)
 * @param value  Output value
 * @return true if positional argument exists and is valid uint32
 */
bool bp_args_positional_uint32(bp_args_t *args, const bp_arg_opt_t *opts, uint32_t pos, uint32_t *value);

/**
 * @brief Get positional argument as float by index.
 * @param args   Parser state
 * @param opts   Option specifications
 * @param pos    Position index
 * @param value  Output value
 * @return true if found and valid
 */
bool bp_args_positional_float(bp_args_t *args, const bp_arg_opt_t *opts, uint32_t pos, float *value);

/**
 * @brief Get positional argument as string by index.
 * @param args    Parser state
 * @param opts    Option specifications
 * @param pos     Position index
 * @param buf     Output buffer
 * @param maxlen  Buffer size
 * @return true if found
 */
bool bp_args_positional_string(bp_args_t *args, const bp_arg_opt_t *opts, uint32_t pos, char *buf, size_t maxlen);

#endif // BP_ARGS_H
