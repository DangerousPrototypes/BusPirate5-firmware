#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <math.h>
#include "pirate.h"
#include "command_struct.h"
#include "ui/ui_prompt.h" //needed for prompt_result struct
#include "ui/ui_parse.h"
#include "ui/ui_const.h"
#include "ui/ui_cmdln.h"
#include "lib/bp_linenoise/ln_cmdreader.h"
#include "lib/bp_number/bp_number.h"

static const struct prompt_result empty_result;

// Helper to convert bp_num_format_t to df_* constants
static inline uint8_t format_to_df(bp_num_format_t fmt) {
    switch (fmt) {
        case BP_NUM_HEX: return df_hex;
        case BP_NUM_BIN: return df_bin;
        case BP_NUM_DEC: 
        default:         return df_dec;
    }
}

bool ui_parse_get_hex(struct prompt_result* result, uint32_t* value) {
    *result = empty_result;
    result->no_value = true;
    *value = 0;

    const char *p = ln_cmdln_current();
    if (!bp_num_hex(&p, value)) {
        return false;
    }
    
    ln_cmdln_advance_to(p);
    result->success = true;
    result->no_value = false;
    return true;
}

bool ui_parse_get_bin(struct prompt_result* result, uint32_t* value) {
    *result = empty_result;
    result->no_value = true;
    *value = 0;

    const char *p = ln_cmdln_current();
    if (!bp_num_bin(&p, value)) {
        return false;
    }
    
    ln_cmdln_advance_to(p);
    result->success = true;
    result->no_value = false;
    return true;
}

bool ui_parse_get_dec(struct prompt_result* result, uint32_t* value) {
    *result = empty_result;
    result->no_value = true;
    *value = 0;

    const char *p = ln_cmdln_current();
    if (!bp_num_dec(&p, value)) {
        return false;
    }
    
    ln_cmdln_advance_to(p);
    result->success = true;
    result->no_value = false;
    return true;
}

// decodes value from the cmdline
// XXXXXX integer
// 0xXXXX hexadecimal
// 0bXXXX bin
bool ui_parse_get_int(struct prompt_result* result, uint32_t* value) {
    *result = empty_result;
    *value = 0;

    const char *p = ln_cmdln_current();
    
    // Check for empty input
    if (*p == '\0') {
        result->no_value = true;
        return false;
    }

    bp_num_format_t fmt;
    if (!bp_num_u32(&p, value, &fmt)) {
        result->no_value = true;
        return false;
    }
    
    ln_cmdln_advance_to(p);
    result->success = true;
    result->no_value = false;
    result->number_format = format_to_df(fmt);
    return true;
}

bool ui_parse_get_string(struct prompt_result* result, char* str, uint8_t* size) {
    uint8_t max_size = *size;

    *result = empty_result;
    result->no_value = true;
    *size = 0;

    const char *p = ln_cmdln_current();
    while (max_size-- && *p != '\0') {
        char c = *p;
        if (c <= ' ') {
            break;
        } else if (c <= '~') {
            *str++ = c;
            (*size)++;
        }
        p++;
        result->success = true;
        result->no_value = false;
    }
    ln_cmdln_advance_to(p);
    
    // Terminate string
    *str = '\0';

    return result->success;
}

// eats up the spaces and commas from the cmdline
void ui_parse_consume_whitespace(void) {
    const char *p = ln_cmdln_current();
    while (*p == ' ' || *p == ',') {
        p++;
    }
    ln_cmdln_advance_to(p);
}

bool ui_parse_get_macro(struct prompt_result* result, uint32_t* value) {
    ui_parse_get_int(result, value);  // get number
    
    const char *p = ln_cmdln_current();
    if (*p == ')') {
        ln_cmdln_advance_to(p + 1);  // consume ')'
        result->success = true;
    } else {
        result->error = true;
    }
    return result->success;
}

// get the repeat from the commandline (if any) XX:repeat
bool ui_parse_get_colon(uint32_t* value) {
    prompt_result result;
    ui_parse_get_delimited_sequence(&result, ':', value);
    return result.success;
}

// get the number of bits from the commandline (if any) XXX.numbit
bool ui_parse_get_dot(uint32_t* value) {
    prompt_result result;
    ui_parse_get_delimited_sequence(&result, '.', value);
    return result.success;
}

// get trailing information for a command, for example :10 or .10
bool ui_parse_get_delimited_sequence(struct prompt_result* result, char delimiter, uint32_t* value) {
    *result = empty_result;

    const char *p = ln_cmdln_current();
    if (*p == delimiter) {
        // Check that the next char is actually numeric
        // prevents eating consecutive .... s
        if (p[1] >= '0' && p[1] <= '9') {
            p++;  // skip delimiter
            ln_cmdln_advance_to(p);
            ui_parse_get_int(result, value);
            result->success = true;
            return true;
        }
    }

    result->no_value = true;
    return false;
}

// some commands have trailing attributes like m 6, o 4 etc
// get as many as specified or error....
bool ui_parse_get_attributes(struct prompt_result* result, uint32_t* attr, uint8_t len) {
    *result = empty_result;
    result->no_value = true;

    for (uint8_t i = 0; i < len; i++) {
        ui_parse_consume_whitespace(); // eat whitechars
        ui_parse_get_uint32(result, &attr[i]);
        if (result->error || result->no_value) {
            return false;
        }
    }

    return true;
}

bool ui_parse_get_bool(struct prompt_result* result, bool* value) {
    *result = empty_result;

    const char *p = ln_cmdln_current();
    char c = *p;

    if (c == '\0') {
        result->no_value = true;
    } else if ((c | 0x20) == 'x') {  // exit
        result->exit = true;
        p++;
    } else if ((c | 0x20) == 'y') {  // yes
        result->success = true;
        (*value) = true;
        p++;
    } else if ((c | 0x20) == 'n') {  // no
        result->success = true;
        (*value) = false;
        p++;
    } else {
        result->error = true;
        p++;  // discard bad char
    }

    ln_cmdln_advance_to(p);
    return true;
}

// get a float from user input
bool ui_parse_get_float(struct prompt_result* result, float* value) {
    *result = empty_result;

    const char *p = ln_cmdln_current();
    char c = *p;

    if (c == '\0') {
        result->no_value = true;
        return true;
    } else if ((c | 0x20) == 'x') {  // exit
        result->exit = true;
        ln_cmdln_advance_to(p + 1);
        return true;
    }

    // Try to parse float (handles integer, decimal, or both)
    if (bp_num_float(&p, value)) {
        result->success = true;
        ln_cmdln_advance_to(p);
        return true;
    }

    result->error = true;
    return false;
}

bool ui_parse_get_uint32(struct prompt_result* result, uint32_t* value) {
    *result = empty_result;

    const char *p = ln_cmdln_current();
    char c = *p;

    if (c == '\0') {
        result->no_value = true;
        return true;
    } else if ((c | 0x20) == 'x') {  // exit
        result->exit = true;
        ln_cmdln_advance_to(p + 1);
        return true;
    } else if (c >= '0' && c <= '9') {
        return ui_parse_get_dec(result, value);
    }

    result->error = true;
    ln_cmdln_advance_to(p + 1);  // discard bad char
    return true;
}

bool ui_parse_get_units(struct prompt_result* result, char* units, uint8_t length, uint8_t* unit_type) {
    uint8_t i = 0;
    *result = empty_result;

    // get the trailing type
    ui_parse_consume_whitespace();

    for (i = 0; i < length; i++) {
        units[i] = 0x00;
    }

    const char *p = ln_cmdln_current();
    i = 0;
    while (i < length && *p != '\0' && *p != ' ') {
        units[i] = (*p | 0x20);  // to lower case
        i++;
        p++;
    }
    ln_cmdln_advance_to(p);
    units[length - 1] = 0x00;

    // TODO: write our own little string compare...
    if (units[0] == 'n' && units[1] == 's') {
        // ns
        (*unit_type) = freq_ns;
    } else if (units[0] == 'u' && units[1] == 's') {
        // us
        (*unit_type) = freq_us;
    } else if (units[0] == 'm' && units[1] == 's') {
        // ms
        (*unit_type) = freq_ms;
    } else if (units[0] == 'h' && units[1] == 'z') {
        // hz
        (*unit_type) = freq_hz;
    } else if (units[0] == 'k' && units[1] == 'h' && units[2] == 'z') {
        // khz
        (*unit_type) = freq_khz;
    } else if (units[0] == 'm' && units[1] == 'h' && units[2] == 'z') {
        // mhz
        (*unit_type) = freq_mhz;
    } else if (units[0] == '%') {
        //%
        (*unit_type) = freq_percent;
    } else {
        result->no_value = true;
        return false;
    }

    result->success = true;
    return true;
}
