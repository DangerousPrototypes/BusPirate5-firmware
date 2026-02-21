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
#include <math.h>
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

// check if a flag is present and get the integer value
//  returns true if flag is present AND has a string value
//  use cmdln_args_find_flag to see if a flag was present with no string value
bool cmdln_args_find_flag_uint32(char flag, command_var_t* arg, uint32_t* value) {
    if (!cmdln_args_find_flag_internal(flag, arg)) {
        return false;
    }

    if (!arg->has_value) {
        return false;
    }

    struct prompt_result result;
    if (!cmdln_args_get_int(&arg->value_pos, &result, value)) {
        arg->error = true;
        return false;
    }

    return true;
}

// check if a flag is present and get the integer value
//  returns true if flag is present AND has a integer value
//  use cmdln_args_find_flag to see if a flag was present with no integer value
bool cmdln_args_find_flag_string(char flag, command_var_t* arg, uint32_t max_len, char* str) {
    if (!cmdln_args_find_flag_internal(flag, arg)) {
        return false;
    }

    if (!arg->has_value) {
        return false;
    }

    if (!cmdln_args_get_string(arg->value_pos, max_len, str)) {
        arg->error = true;
        return false;
    }

    return true;
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

bool cmdln_args_uint32_by_position(uint32_t pos, uint32_t* value) {
    char c;
    uint32_t rptr = 0;
// start at beginning of command range
#ifdef UI_CMDLN_ARGS_DEBUG
    printf("Looking for uint in pos %d\r\n", pos);
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
            struct prompt_result result;
            if (cmdln_args_get_int(&rptr, &result, value)) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

bool cmdln_args_float_by_position(uint32_t pos, float* value) {
    char c;
    uint32_t rptr = 0;
    uint32_t ipart = 0, dpart = 0;
// start at beginning of command range
#ifdef UI_CMDLN_ARGS_DEBUG
    printf("Looking for uint in pos %d\r\n", pos);
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
            // before decimal
            if (!cmdln_try_peek(rptr, &c)) {
                return false;
            }
            if( c=='-') {
                //encountered flag, end of positional arguments
                return false;
            }
            if ((c >= '0') && (c <= '9')) // first part of decimal
            {
                struct prompt_result result;
                if (!cmdln_args_get_int(&rptr, &result, &ipart)) {
                    return false;
                }
                // printf("ipart: %d\r\n", ipart);
            }

            uint32_t dpart_len = 0;
            if (cmdln_try_peek(rptr, &c)) {
                if (c == '.' || c == ',') { // second part of decimal
                    rptr++;                 // discard
                    dpart_len = rptr;       // track digits
                    struct prompt_result result;
                    if (!cmdln_args_get_int(&rptr, &result, &dpart)) {
                        // printf("No decimal part found\r\n");
                    }
                    dpart_len = rptr - dpart_len;
                    // printf("dpart: %d, dpart_len: %d\r\n", dpart, dpart_len);
                }
            }
            (*value) = (float)ipart;
            (*value) += ((float)dpart / (float)pow(10, dpart_len));
            // printf("value: %f\r\n", *value);
            return true;
        }
    }
    return false;
}

// Get a direct pointer to everything after the command name (position 0).
// Skips the command word and leading whitespace, returns pointer into ln_cmdln.buf.
bool cmdln_args_remainder(const char **out, size_t *len) {
    uint32_t rptr = 0;

    // Skip command word (position 0 non-whitespace)
    if (!cmdln_consume_white_space(&rptr, false)) return false;  // skip leading ws
    if (!cmdln_consume_white_space(&rptr, true))  return false;  // skip command word
    if (!cmdln_consume_white_space(&rptr, false)) {
        // No trailing content after command
        return false;
    }

    uint32_t start = command_info.startptr + rptr;
    uint32_t end   = command_info.endptr + 1;  // endptr is inclusive
    if (start >= end) return false;

    *out = ln_cmdln.buf + start;
    *len = end - start;
    return true;
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

//get first argument following command, search in a list of actions, error if not found
// returns false if action is found, true if no action is found or error
bool cmdln_args_get_action(const struct cmdln_action_t* action_list, size_t count_of_action_list, uint32_t *action) {
    command_var_t arg;
    char arg_str[9];
    bool action_found = false; // invalid by default

    // action is the first argument (read/write/probe/erase/etc)
    if(!cmdln_args_string_by_position(1, sizeof(arg_str), arg_str)) return true; // no action found, return true to indicate no error

    // get action from struct
    for (uint8_t i = 0; i < count_of_action_list; i++) {
        if (strcmp(arg_str, action_list[i].verb) == 0) {
            (*action) = action_list[i].action;
            action_found = true; // found the action
            break;
        }
    }

    if (!action_found) {
        if(strlen(arg_str) > 0) printf("\r\nInvalid action: %s\r\n\r\n", arg_str);
        return true; // invalid action
    }

    return false;
}