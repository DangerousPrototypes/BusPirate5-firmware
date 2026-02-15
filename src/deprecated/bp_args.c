/**
 * @file bp_args.c
 * @brief Command line argument parsing implementation.
 */

#include "bp_args.h"
#include "lib/bp_number/bp_number.h"
#include <string.h>

/*
 * =============================================================================
 * Internal helpers
 * =============================================================================
 */

// Skip whitespace, return new position
static const char *skip_whitespace(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) {
        p++;
    }
    return p;
}

// Skip non-whitespace (a token), return new position
static const char *skip_token(const char *p, const char *end) {
    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0' && *p != '=') {
        p++;
    }
    return p;
}

// Get token length (stops at whitespace, '=', or end)
static size_t token_length(const char *p, const char *end) {
    const char *start = p;
    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0' && *p != '=') {
        p++;
    }
    return p - start;
}

// Compare string with non-null-terminated token
static bool token_equals(const char *token, size_t len, const char *str) {
    size_t slen = strlen(str);
    if (len != slen) return false;
    return memcmp(token, str, len) == 0;
}

// Find option by short name
static const bp_arg_opt_t *find_opt_short(const bp_arg_opt_t *opts, char c) {
    for (int i = 0; opts[i].long_name || opts[i].short_name; i++) {
        if (opts[i].short_name == c) {
            return &opts[i];
        }
    }
    return NULL;
}

// Find option by long name
static const bp_arg_opt_t *find_opt_long(const bp_arg_opt_t *opts, const char *name, size_t len) {
    for (int i = 0; opts[i].long_name || opts[i].short_name; i++) {
        if (opts[i].long_name && token_equals(name, len, opts[i].long_name)) {
            return &opts[i];
        }
    }
    return NULL;
}

/*
 * =============================================================================
 * Core API
 * =============================================================================
 */

void bp_args_init(bp_args_t *args, const char *cmdline, size_t len) {
    args->start = cmdline;
    args->end = cmdline + len;
    args->optarg = NULL;
    args->optarg_len = 0;
    args->optopt = 0;
    args->errmsg = NULL;
    args->done_options = false;
    
    // Skip the command name (first token)
    args->str = skip_whitespace(cmdline, args->end);
    args->str = skip_token(args->str, args->end);
    args->str = skip_whitespace(args->str, args->end);
}

int bp_args_next(bp_args_t *args, const bp_arg_opt_t *opts) {
    args->optarg = NULL;
    args->optarg_len = 0;
    args->optopt = 0;
    args->errmsg = NULL;
    
    // Skip whitespace
    args->str = skip_whitespace(args->str, args->end);
    
    // Check for end of input
    if (args->str >= args->end || *args->str == '\0') {
        return -1;
    }
    
    // If we've seen -- or a non-option, we're done with options
    if (args->done_options) {
        return -1;
    }
    
    // Check for -- (end of options marker)
    if (args->str + 1 < args->end && args->str[0] == '-' && args->str[1] == '-') {
        if (args->str + 2 >= args->end || args->str[2] == ' ' || args->str[2] == '\0') {
            args->str += 2;
            args->str = skip_whitespace(args->str, args->end);
            args->done_options = true;
            return -1;
        }
        
        // Long option: --name or --name=value
        const char *opt_start = args->str + 2;
        const char *opt_end = opt_start;
        
        // Find end of option name (stop at '=' or whitespace)
        while (opt_end < args->end && *opt_end != '=' && *opt_end != ' ' && 
               *opt_end != '\t' && *opt_end != '\0') {
            opt_end++;
        }
        
        size_t opt_len = opt_end - opt_start;
        const bp_arg_opt_t *opt = find_opt_long(opts, opt_start, opt_len);
        
        if (!opt) {
            args->errmsg = "unknown option";
            args->optopt = '?';
            args->str = skip_token(args->str, args->end);
            return '?';
        }
        
        args->optopt = opt->short_name ? opt->short_name : '?';
        
        // Check for =value
        if (opt_end < args->end && *opt_end == '=') {
            args->optarg = opt_end + 1;
            args->optarg_len = token_length(args->optarg, args->end);
            args->str = args->optarg + args->optarg_len;
            
            if (opt->arg_type == BP_ARG_NONE) {
                args->errmsg = "option takes no argument";
                return '?';
            }
        } else {
            args->str = opt_end;
            args->str = skip_whitespace(args->str, args->end);
            
            // Check for separate argument
            if (opt->arg_type == BP_ARG_REQUIRED) {
                if (args->str >= args->end || *args->str == '\0' || *args->str == '-') {
                    args->errmsg = "option requires argument";
                    return '?';
                }
                args->optarg = args->str;
                args->optarg_len = token_length(args->optarg, args->end);
                args->str = args->optarg + args->optarg_len;
            } else if (opt->arg_type == BP_ARG_OPTIONAL) {
                // For optional, only take value if it doesn't look like an option
                if (args->str < args->end && *args->str != '-' && *args->str != '\0') {
                    args->optarg = args->str;
                    args->optarg_len = token_length(args->optarg, args->end);
                    args->str = args->optarg + args->optarg_len;
                }
            }
        }
        
        return args->optopt;
    }
    
    // Check for short option: -x or -xyz (bundled)
    if (args->str[0] == '-' && args->str + 1 < args->end && 
        args->str[1] != '-' && args->str[1] != ' ' && args->str[1] != '\0') {
        
        char opt_char = args->str[1];
        const bp_arg_opt_t *opt = find_opt_short(opts, opt_char);
        
        if (!opt) {
            args->errmsg = "unknown option";
            args->optopt = opt_char;
            args->str += 2;
            return '?';
        }
        
        args->optopt = opt_char;
        
        // Check what follows the option character
        if (args->str + 2 < args->end && args->str[2] != ' ' && args->str[2] != '\0') {
            // Something follows: could be bundled options or argument value
            if (opt->arg_type != BP_ARG_NONE) {
                // Take rest as argument value
                args->optarg = args->str + 2;
                args->optarg_len = token_length(args->optarg, args->end);
                args->str = args->optarg + args->optarg_len;
            } else {
                // Just move past this option, leave bundled options for next call
                // Actually, for simplicity, treat -abc as -a -b -c would require state
                // Let's treat -fvalue as -f with value, and require -a -b for bundling
                args->str += 2;
            }
        } else {
            // Nothing follows immediately
            args->str += 2;
            args->str = skip_whitespace(args->str, args->end);
            
            if (opt->arg_type == BP_ARG_REQUIRED) {
                if (args->str >= args->end || *args->str == '\0' || *args->str == '-') {
                    args->errmsg = "option requires argument";
                    return '?';
                }
                args->optarg = args->str;
                args->optarg_len = token_length(args->optarg, args->end);
                args->str = args->optarg + args->optarg_len;
            } else if (opt->arg_type == BP_ARG_OPTIONAL) {
                if (args->str < args->end && *args->str != '-' && *args->str != '\0') {
                    args->optarg = args->str;
                    args->optarg_len = token_length(args->optarg, args->end);
                    args->str = args->optarg + args->optarg_len;
                }
            }
        }
        
        return args->optopt;
    }
    
    // Not an option - we're done with option parsing
    args->done_options = true;
    return -1;
}

bool bp_args_positional(bp_args_t *args, const char **arg, size_t *len) {
    args->str = skip_whitespace(args->str, args->end);
    
    if (args->str >= args->end || *args->str == '\0') {
        return false;
    }
    
    // Skip flags (shouldn't happen if called after bp_args_next returns -1, but be safe)
    if (*args->str == '-') {
        return false;
    }
    
    *arg = args->str;
    *len = token_length(args->str, args->end);
    args->str = *arg + *len;
    
    return *len > 0;
}

/*
 * =============================================================================
 * Value extraction helpers
 * =============================================================================
 */

bool bp_args_get_uint32(bp_args_t *args, uint32_t *value) {
    if (!args->optarg || args->optarg_len == 0) {
        return false;
    }
    
    const char *p = args->optarg;
    const char *end = args->optarg + args->optarg_len;
    bp_num_format_t fmt;
    
    // Create temporary null-terminated copy for bp_num_u32
    // Actually bp_num_u32 works with pointer advancement, not null termination
    if (bp_num_u32(&p, value, &fmt)) {
        return p <= end;  // Make sure we didn't read past the argument
    }
    return false;
}

bool bp_args_get_int32(bp_args_t *args, int32_t *value) {
    if (!args->optarg || args->optarg_len == 0) {
        return false;
    }
    
    const char *p = args->optarg;
    const char *end = args->optarg + args->optarg_len;
    bp_num_format_t fmt;
    
    if (bp_num_i32(&p, value, &fmt)) {
        return p <= end;
    }
    return false;
}

bool bp_args_get_float(bp_args_t *args, float *value) {
    if (!args->optarg || args->optarg_len == 0) {
        return false;
    }
    
    const char *p = args->optarg;
    const char *end = args->optarg + args->optarg_len;
    
    if (bp_num_float(&p, value)) {
        return p <= end;
    }
    return false;
}

bool bp_args_get_string(bp_args_t *args, char *buf, size_t maxlen) {
    if (!args->optarg || args->optarg_len == 0 || maxlen == 0) {
        if (maxlen > 0) buf[0] = '\0';
        return false;
    }
    
    size_t copy_len = args->optarg_len;
    if (copy_len >= maxlen) {
        copy_len = maxlen - 1;
    }
    
    memcpy(buf, args->optarg, copy_len);
    buf[copy_len] = '\0';
    
    return true;
}

/*
 * =============================================================================
 * Convenience functions (for compatibility with old API)
 * =============================================================================
 */

bool bp_args_has_flag(bp_args_t *args, const bp_arg_opt_t *opts, char flag) {
    // Re-init parser to scan from beginning
    bp_args_t scan;
    scan.str = args->start;
    scan.start = args->start;
    scan.end = args->end;
    scan.optarg = NULL;
    scan.optarg_len = 0;
    scan.optopt = 0;
    scan.errmsg = NULL;
    scan.done_options = false;
    
    // Skip command name
    scan.str = skip_whitespace(scan.str, scan.end);
    scan.str = skip_token(scan.str, scan.end);
    scan.str = skip_whitespace(scan.str, scan.end);
    
    int opt;
    while ((opt = bp_args_next(&scan, opts)) != -1) {
        if (opt == flag) {
            return true;
        }
    }
    return false;
}

bool bp_args_find_uint32(bp_args_t *args, const bp_arg_opt_t *opts, char flag, uint32_t *value) {
    bp_args_t scan;
    scan.str = args->start;
    scan.start = args->start;
    scan.end = args->end;
    scan.optarg = NULL;
    scan.optarg_len = 0;
    scan.optopt = 0;
    scan.errmsg = NULL;
    scan.done_options = false;
    
    scan.str = skip_whitespace(scan.str, scan.end);
    scan.str = skip_token(scan.str, scan.end);
    scan.str = skip_whitespace(scan.str, scan.end);
    
    int opt;
    while ((opt = bp_args_next(&scan, opts)) != -1) {
        if (opt == flag) {
            return bp_args_get_uint32(&scan, value);
        }
    }
    return false;
}

bool bp_args_find_string(bp_args_t *args, const bp_arg_opt_t *opts, char flag, char *buf, size_t maxlen) {
    bp_args_t scan;
    scan.str = args->start;
    scan.start = args->start;
    scan.end = args->end;
    scan.optarg = NULL;
    scan.optarg_len = 0;
    scan.optopt = 0;
    scan.errmsg = NULL;
    scan.done_options = false;
    
    scan.str = skip_whitespace(scan.str, scan.end);
    scan.str = skip_token(scan.str, scan.end);
    scan.str = skip_whitespace(scan.str, scan.end);
    
    int opt;
    while ((opt = bp_args_next(&scan, opts)) != -1) {
        if (opt == flag) {
            return bp_args_get_string(&scan, buf, maxlen);
        }
    }
    if (maxlen > 0) buf[0] = '\0';
    return false;
}

bool bp_args_positional_uint32(bp_args_t *args, const bp_arg_opt_t *opts, uint32_t pos, uint32_t *value) {
    bp_args_t scan;
    scan.str = args->start;
    scan.start = args->start;
    scan.end = args->end;
    scan.optarg = NULL;
    scan.optarg_len = 0;
    scan.optopt = 0;
    scan.errmsg = NULL;
    scan.done_options = false;
    
    scan.str = skip_whitespace(scan.str, scan.end);
    scan.str = skip_token(scan.str, scan.end);
    scan.str = skip_whitespace(scan.str, scan.end);
    
    // Skip all options
    if (opts) {
        while (bp_args_next(&scan, opts) != -1) {
            // Just consume options
        }
    }
    
    // Get positional argument by index
    const char *arg;
    size_t len;
    uint32_t idx = 0;
    
    while (bp_args_positional(&scan, &arg, &len)) {
        if (idx == pos) {
            const char *p = arg;
            bp_num_format_t fmt;
            if (bp_num_u32(&p, value, &fmt)) {
                return p <= arg + len;
            }
            return false;
        }
        idx++;
    }
    return false;
}

bool bp_args_positional_float(bp_args_t *args, const bp_arg_opt_t *opts, uint32_t pos, float *value) {
    bp_args_t scan;
    scan.str = args->start;
    scan.start = args->start;
    scan.end = args->end;
    scan.optarg = NULL;
    scan.optarg_len = 0;
    scan.optopt = 0;
    scan.errmsg = NULL;
    scan.done_options = false;
    
    scan.str = skip_whitespace(scan.str, scan.end);
    scan.str = skip_token(scan.str, scan.end);
    scan.str = skip_whitespace(scan.str, scan.end);
    
    if (opts) {
        while (bp_args_next(&scan, opts) != -1) {}
    }
    
    const char *arg;
    size_t len;
    uint32_t idx = 0;
    
    while (bp_args_positional(&scan, &arg, &len)) {
        if (idx == pos) {
            const char *p = arg;
            if (bp_num_float(&p, value)) {
                return p <= arg + len;
            }
            return false;
        }
        idx++;
    }
    return false;
}

bool bp_args_positional_string(bp_args_t *args, const bp_arg_opt_t *opts, uint32_t pos, char *buf, size_t maxlen) {
    bp_args_t scan;
    scan.str = args->start;
    scan.start = args->start;
    scan.end = args->end;
    scan.optarg = NULL;
    scan.optarg_len = 0;
    scan.optopt = 0;
    scan.errmsg = NULL;
    scan.done_options = false;
    
    scan.str = skip_whitespace(scan.str, scan.end);
    scan.str = skip_token(scan.str, scan.end);
    scan.str = skip_whitespace(scan.str, scan.end);
    
    if (opts) {
        while (bp_args_next(&scan, opts) != -1) {}
    }
    
    const char *arg;
    size_t len;
    uint32_t idx = 0;
    
    while (bp_args_positional(&scan, &arg, &len)) {
        if (idx == pos) {
            size_t copy_len = len;
            if (copy_len >= maxlen) copy_len = maxlen - 1;
            memcpy(buf, arg, copy_len);
            buf[copy_len] = '\0';
            return true;
        }
        idx++;
    }
    if (maxlen > 0) buf[0] = '\0';
    return false;
}
