/**
 * @file bp_args_compat.c
 * @brief Compatibility layer implementation.
 */

#include "bp_args_compat.h"
#include "lib/bp_number/bp_number.h"
#include <string.h>

/*
 * Dynamic option table for compatibility mode.
 * Since the old API allows any single-char flag, we need to handle this.
 * We'll scan the input to find flags dynamically.
 */

// Internal: scan for a specific flag in the command line
static bool compat_scan_for_flag(char flag, const char **arg_start, size_t *arg_len) {
    const char *p = ln_cmdln_current();
    size_t remaining = ln_cmdln_remaining();
    const char *end = p + remaining;
    
    // Skip command name
    while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
    
    while (p < end) {
        // Skip whitespace
        while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
        if (p >= end || *p == '\0') break;
        
        // Check for short option
        if (*p == '-' && p + 1 < end && p[1] != '-') {
            char opt_char = p[1];
            p += 2;
            
            // Case-insensitive flag match
            if ((opt_char | 0x20) == (flag | 0x20)) {
                // Check for attached value (-fvalue)
                if (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0' && *p != '-') {
                    *arg_start = p;
                    const char *arg_end = p;
                    while (arg_end < end && *arg_end != ' ' && *arg_end != '\t' && 
                           *arg_end != ',' && *arg_end != '\0') {
                        arg_end++;
                    }
                    *arg_len = arg_end - p;
                    return true;
                }
                
                // Skip whitespace for separate value
                while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
                
                // Check for value (if not another flag)
                if (p < end && *p != '-' && *p != '\0') {
                    *arg_start = p;
                    const char *arg_end = p;
                    while (arg_end < end && *arg_end != ' ' && *arg_end != '\t' && 
                           *arg_end != ',' && *arg_end != '\0') {
                        arg_end++;
                    }
                    *arg_len = arg_end - p;
                    return true;
                }
                
                // Flag found but no value
                *arg_start = NULL;
                *arg_len = 0;
                return true;
            }
            
            // Skip any attached value for other flags
            while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
            continue;
        }
        
        // Check for long option --name or --name=value
        if (*p == '-' && p + 1 < end && p[1] == '-') {
            p += 2;
            // Skip to end of option
            while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
            continue;
        }
        
        // Skip non-option token (positional argument)
        while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
    }
    
    return false;
}

// Internal: get positional argument by index
static bool compat_get_positional(uint32_t pos, const char **arg_start, size_t *arg_len) {
    const char *p = ln_cmdln_current();
    size_t remaining = ln_cmdln_remaining();
    const char *end = p + remaining;
    uint32_t found = 0;
    
    // Skip command name
    while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
    
    while (p < end) {
        // Skip whitespace
        while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
        if (p >= end || *p == '\0') break;
        
        // Skip flags
        if (*p == '-') {
            // Skip short option and any attached value
            if (p + 1 < end && p[1] != '-') {
                p += 2;
                // Skip attached value
                while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
                // Skip whitespace
                while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
                // Skip separate value if present and not a flag
                if (p < end && *p != '-' && *p != '\0') {
                    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
                }
                continue;
            }
            // Skip long option
            if (p + 1 < end && p[1] == '-') {
                p += 2;
                while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
                // Skip whitespace
                while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) p++;
                // Skip value if not a flag
                if (p < end && *p != '-' && *p != '\0') {
                    while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
                }
                continue;
            }
        }
        
        // This is a positional argument
        const char *token_start = p;
        while (p < end && *p != ' ' && *p != '\t' && *p != ',' && *p != '\0') p++;
        size_t token_len = p - token_start;
        
        if (found == pos) {
            *arg_start = token_start;
            *arg_len = token_len;
            return true;
        }
        found++;
    }
    
    return false;
}

/*
 * =============================================================================
 * Public compatibility API
 * =============================================================================
 */

bool bp_compat_find_flag(char flag) {
    const char *arg;
    size_t len;
    return compat_scan_for_flag(flag, &arg, &len);
}

bool bp_compat_find_flag_uint32(char flag, command_var_t* arg, uint32_t* value) {
    const char *arg_str;
    size_t arg_len;
    
    if (arg) {
        arg->has_arg = false;
        arg->has_value = false;
        arg->error = false;
    }
    
    if (!compat_scan_for_flag(flag, &arg_str, &arg_len)) {
        return false;
    }
    
    if (arg) {
        arg->has_arg = true;
    }
    
    if (!arg_str || arg_len == 0) {
        return false;
    }
    
    if (arg) {
        arg->has_value = true;
    }
    
    // Parse value
    const char *p = arg_str;
    bp_num_format_t fmt;
    if (!bp_num_u32(&p, value, &fmt)) {
        if (arg) arg->error = true;
        return false;
    }
    
    if (arg) {
        // Set number format for compatibility
        switch (fmt) {
            case BP_NUM_HEX: arg->number_format = 1; break;  // df_hex
            case BP_NUM_BIN: arg->number_format = 2; break;  // df_bin
            default: arg->number_format = 0; break;          // df_dec
        }
    }
    
    return true;
}

bool bp_compat_find_flag_string(char flag, command_var_t* arg, uint32_t max_len, char* str) {
    const char *arg_str;
    size_t arg_len;
    
    if (arg) {
        arg->has_arg = false;
        arg->has_value = false;
        arg->error = false;
    }
    
    if (max_len > 0) str[0] = '\0';
    
    if (!compat_scan_for_flag(flag, &arg_str, &arg_len)) {
        return false;
    }
    
    if (arg) {
        arg->has_arg = true;
    }
    
    if (!arg_str || arg_len == 0) {
        return false;
    }
    
    if (arg) {
        arg->has_value = true;
    }
    
    // Copy string
    size_t copy_len = arg_len;
    if (copy_len >= max_len) copy_len = max_len - 1;
    memcpy(str, arg_str, copy_len);
    str[copy_len] = '\0';
    
    return true;
}

bool bp_compat_uint32_by_position(uint32_t pos, uint32_t* value) {
    const char *arg;
    size_t len;
    
    if (!compat_get_positional(pos, &arg, &len)) {
        return false;
    }
    
    const char *p = arg;
    bp_num_format_t fmt;
    return bp_num_u32(&p, value, &fmt);
}

bool bp_compat_float_by_position(uint32_t pos, float* value) {
    const char *arg;
    size_t len;
    
    if (!compat_get_positional(pos, &arg, &len)) {
        return false;
    }
    
    const char *p = arg;
    return bp_num_float(&p, value);
}

bool bp_compat_string_by_position(uint32_t pos, uint32_t max_len, char* str) {
    const char *arg;
    size_t len;
    
    if (max_len > 0) str[0] = '\0';
    
    if (!compat_get_positional(pos, &arg, &len)) {
        return false;
    }
    
    size_t copy_len = len;
    if (copy_len >= max_len) copy_len = max_len - 1;
    memcpy(str, arg, copy_len);
    str[copy_len] = '\0';
    
    return true;
}
