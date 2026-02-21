/**
 * @file ui_cmdln.c
 * @brief Command line argument parsing implementation.
 * @details Provides argument parsing using linenoise linear buffer:
 *          - Command chaining with delimiters (; || &&)
 *          - Argument parsing (flags, positions, types)
 *          - Multiple number formats (hex, decimal, binary)
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_const.h"
#include "ui/ui_prompt.h"
#include "ui/ui_cmdln.h"
#include "lib/bp_linenoise/ln_cmdreader.h"

struct _command_info_t command_info; // the current command and position in the buffer

static const struct prompt_result empty_result;

// consume white space (0x20, space)
//  non_white_space = true, consume non-white space characters (not space)
static bool cmdln_consume_white_space(uint32_t* rptr, bool non_white_space) {
    // consume white space
    while (true) {
        char c;
        // no more characters
        if (!(command_info.endptr >= (command_info.startptr + (*rptr)) &&
              cmdln_try_peek(command_info.startptr + (*rptr), &c))) {
            return false;
        }
        if ((!non_white_space && c == ' ')      // consume white space
            || (non_white_space && c != ' ')) { // consume non-whitespace
            // printf("Whitespace at %d\r\n", cp->startptr+rptr);
            (*rptr)++;
        } else {
            break;
        }
    }
    return true;
}

// internal function to take copy string from start position to next space or end of buffer
// notice, we do not pass rptr by reference, so it is not updated
static bool cmdln_args_get_string(uint32_t rptr, uint32_t max_len, char* string) {
    char c;
    for (uint32_t i = 0; i < max_len; i++) {
        // no more characters
        if ((!(command_info.endptr >= command_info.startptr + rptr &&
               cmdln_try_peek(command_info.startptr + rptr, &c))) ||
            c == ' ' || i == (max_len - 1)) {
            string[i] = 0x00;
            if (i == 0) {
                return false;
            } else {
                return true;
            }
        }
        string[i] = c;
        rptr++;
    }
}

// parse a hex value from the first digit
// notice, we do not pass rptr by reference, so it is not updated
static bool cmdln_args_get_hex(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
    char c;

    //*result=empty_result;
    result->no_value = true;
    (*value) = 0;

    while (command_info.endptr >= command_info.startptr + (*rptr) &&
           cmdln_try_peek(command_info.startptr + (*rptr), &c)) { // peek at next char
        if (((c >= '0') && (c <= '9'))) {
            (*value) <<= 4;
            (*value) += (c - 0x30);
        } else if (((c | 0x20) >= 'a') && ((c | 0x20) <= 'f')) {
            (*value) <<= 4;
            c |= 0x20;              // to lowercase
            (*value) += (c - 0x57); // 0x61 ('a') -0xa
        } else {
            break;
        }
        (*rptr)++;
        result->success = true;
        result->no_value = false;
    }
    return result->success;
}

// parse a bin value from the first digit
// notice, we do not pass rptr by reference, so it is not updated
static bool cmdln_args_get_bin(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
    char c;
    //*result=empty_result;
    result->no_value = true;
    (*value) = 0;

    while (command_info.endptr >= command_info.startptr + (*rptr) &&
           cmdln_try_peek(command_info.startptr + (*rptr), &c)) {
        if ((c < '0') || (c > '1')) {
            break;
        }
        (*value) <<= 1;
        (*value) += c - 0x30;
        (*rptr)++;
        result->success = true;
        result->no_value = false;
    }

    return result->success;
}

// parse a decimal value from the first digit
// notice, we do not pass rptr by reference, so it is not updated
static bool cmdln_args_get_dec(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
    char c;
    //*result=empty_result;
    result->no_value = true;
    (*value) = 0;

    while (command_info.endptr >= command_info.startptr + (*rptr) &&
           cmdln_try_peek(command_info.startptr + (*rptr), &c)) // peek at next char
    {
        if ((c < '0') || (c > '9')) // if there is a char, and it is in range
        {
            break;
        }
        (*value) *= 10;
        (*value) += (c - 0x30);
        (*rptr)++;
        result->success = true;
        result->no_value = false;
    }
    return result->success;
}

// decodes value from the cmdline
// XXXXXX integer
// 0xXXXX hexadecimal
// 0bXXXX bin
bool cmdln_args_get_int(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
    bool r1, r2;
    char p1, p2;

    *result = empty_result;
    r1 = cmdln_try_peek(command_info.startptr + (*rptr), &p1);
    r2 = cmdln_try_peek(command_info.startptr + (*rptr) + 1, &p2);
    if (!r1 || (p1 == 0x00)) { // no data, end of data, or no value entered on prompt
        result->no_value = true;
        return false;
    }

    if (r2 && (p2 | 0x20) == 'x') { // HEX
        (*rptr) += 2;
        cmdln_args_get_hex(rptr, result, value);
        result->number_format = df_hex;    // whatever from ui_const
    } else if (r2 && (p2 | 0x20) == 'b') { // BIN
        (*rptr) += 2;
        cmdln_args_get_bin(rptr, result, value);
        result->number_format = df_bin; // whatever from ui_const
    } else {                            // DEC
        cmdln_args_get_dec(rptr, result, value);
        result->number_format = df_dec; // whatever from ui_const
    }
    return result->success;
}

static bool cmdln_args_find_flag_internal(char flag, command_var_t* arg) {
    uint32_t rptr = 0;
    char flag_c;
    char dash_c;
    arg->error = false;
    arg->has_arg = false;
    while (command_info.endptr >= (command_info.startptr + rptr + 1) && cmdln_try_peek(rptr, &dash_c) &&
           cmdln_try_peek(rptr + 1, &flag_c)) {
        if (dash_c == '-' && flag_c == flag) {
            arg->has_arg = true;
            if ((!cmdln_consume_white_space(&rptr, true))     // move past the flag characters
                || (!cmdln_consume_white_space(&rptr, false)) // move past spaces. @end of buffer, next flag, or value
                || (cmdln_try_peek(rptr, &dash_c) && dash_c == '-')) // next argument, no value
            {
                // printf("No value for flag %c\r\n", flag);
                arg->has_value = false;
                return true;
            }
            // printf("Value for flag %c\r\n", flag);
            arg->has_value = true;
            arg->value_pos = rptr;
            return true;
        }
        rptr++;
    }
    // printf("Flag %c not found\r\n", flag);
    return false;
}

// check if a -f(lag) is present. Value is don't care.
// returns true if flag is present
bool cmdln_args_find_flag(char flag) {
    command_var_t arg;
    if (!cmdln_args_find_flag_internal(flag, &arg)) {
        return false;
    }
    return true;
}

bool cmdln_args_string_by_position(uint32_t pos, uint32_t max_len, char* str) {
    char c;
    uint32_t rptr = 0;
    memset(str, 0x00, max_len);
// start at beginning of command range
#ifdef UI_CMDLN_ARGS_DEBUG
    printf("Looking for string in pos %d\r\n", pos);
#endif
    for (uint32_t i = 0; i < pos + 1; i++) {
        // consume white space
        if (!cmdln_consume_white_space(&rptr, false)) {
            return false;
        }
        // consume non-white space
        if (i != pos) {
            if (!cmdln_consume_white_space(&rptr, true)) { // consume non-white space
                return false;
            }
        } else {
            cmdln_try_peek(command_info.startptr + (rptr), &c);
            //see if this is a argument or a flag, reject flags
            if (c=='-') {
                return false;
            }
            struct prompt_result result;
            if (cmdln_args_get_string(rptr, max_len, str)) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

// finds the next command in user input
// could be a single command ending in 0x00
// could be multiple commands chained with ; || &&
// sets the internal current command pointers to avoid reading into the next or previous commands
// returns true if a command is found
bool cmdln_find_next_command(struct _command_info_t* cp) {
    uint32_t i = 0;
    char c, d;
    cp->startptr = cp->endptr = cp->nextptr; // should be zero on first call, use previous value for subsequent calls

    if (!cmdln_try_peek(cp->endptr, &c)) {
#ifdef UI_CMDLN_ARGS_DEBUG
        printf("End of command line input at %d\r\n", cp->endptr);
#endif
        cp->delimiter = false; // 0 = end of command input
        return false;
    }
    memset(cp->command, 0x00, 9);
    while (true) {
        bool got_pos1 = cmdln_try_peek(cp->endptr, &c);
        
        //consume white space
        if(got_pos1 && c == ' ') {
            cp->endptr++;
            continue;
        }
        
        bool got_pos2 = cmdln_try_peek(cp->endptr + 1, &d);
        if (!got_pos1) {
#ifdef UI_CMDLN_ARGS_DEBUG
            printf("Last/only command line input: pos1=%d, c=%d\r\n", got_pos1, c);
#endif
            cp->delimiter = false; // 0 = end of command input
            cp->nextptr = cp->endptr;
            goto cmdln_find_next_command_success;
        } else if (c == ';') {
#ifdef UI_CMDLN_ARGS_DEBUG
            printf("Got end of command: ; position: %d, \r\n", cp->endptr);
#endif
            cp->delimiter = ';';
            cp->nextptr = cp->endptr + 1;
            goto cmdln_find_next_command_success;
        } else if (got_pos2 && ((c == '|' && d == '|') || (c == '&' && d == '&'))) {
#ifdef UI_CMDLN_ARGS_DEBUG
            printf("Got end of command: %c position: %d, \r\n", c, cp->endptr);
#endif
            cp->delimiter = c;
            cp->nextptr = cp->endptr + 2;
            goto cmdln_find_next_command_success;
        } else if (i < 8) {
            cp->command[i] = c;
            if (c == ' ') {
                i = 8; // stop at space if possible
            }
            i++;
        }
        cp->endptr++;
    }
cmdln_find_next_command_success:
    cp->endptr--;
    command_info.startptr = cp->startptr;
    command_info.endptr = cp->endptr;
    return true;
}

