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

// the command line struct with buffer and several pointers
struct _command_line cmdln;          // everything the user entered before <enter>
struct _command_info_t command_info; // the current command and position in the buffer
static const struct prompt_result empty_result;

void cmdln_init(void) {
    for (uint32_t i = 0; i < UI_CMDBUFFSIZE; i++) {
        cmdln.buf[i] = 0x00;
    }
    cmdln.wptr = 0;
    cmdln.rptr = 0;
    cmdln.histptr = 0;
    cmdln.cursptr = 0;
}
// pointer update, rolls over
uint32_t cmdln_pu(uint32_t i) {
    return ((i) & (UI_CMDBUFFSIZE - 1));
}

void cmdln_get_command_pointer(struct _command_pointer* cp) {
    cp->wptr = cmdln.wptr;
    cp->rptr = cmdln.rptr;
}

bool cmdln_try_add(char* c) {
    // TODO: leave one space for 0x00 command seperator????
    if (cmdln_pu(cmdln.wptr + 1) == cmdln_pu(cmdln.rptr)) {
        return false;
    }
    cmdln.buf[cmdln.wptr] = (*c);
    cmdln.wptr = cmdln_pu(cmdln.wptr + 1);
    return true;
}

bool cmdln_try_remove(char* c) {
    if (cmdln_pu(cmdln.rptr) == cmdln_pu(cmdln.wptr)) {
        return false;
    }

    (*c) = cmdln.buf[cmdln.rptr];
    cmdln.rptr = cmdln_pu(cmdln.rptr + 1);
    return true;
}

bool cmdln_try_peek(uint32_t i, char* c) {
    if (cmdln_pu(cmdln.rptr + i) == cmdln_pu(cmdln.wptr)) {
        return false;
    }

    (*c) = cmdln.buf[cmdln_pu(cmdln.rptr + i)];

    if ((*c) == 0x00) {
        return false;
    }

    return true;
}

bool cmdln_try_peek_pointer(struct _command_pointer* cp, uint32_t i, char* c) {
    if (cmdln_pu(cp->rptr + i) == cmdln_pu(cp->wptr)) {
        return false;
    }

    (*c) = cmdln.buf[cmdln_pu(cp->rptr + i)];
    return true;
}

bool cmdln_try_discard(uint32_t i) {
    // this isn't very effective, maybe just not use it??
    // if(cmdln_pu(cmdln.rptr+i) == cmdln_pu(cmdln.wprt))
    //{
    //    return false;
    //}
    cmdln.rptr = cmdln_pu(cmdln.rptr + i);
    return true;
}

bool cmdln_next_buf_pos(void) {
    cmdln.rptr = cmdln.wptr;
    cmdln.cursptr = cmdln.wptr;
    cmdln.histptr = 0;
}

// These are new functions to ease argument and options parsing
//  Isolate the next command between the current read pointer and 0x00, (?test?) end of pointer, ; || && (next commmand)
//  functions to move within the single command range

uint32_t cmdln_get_length_pointer(struct _command_line* cp) {
    if (cp->rptr > cp->wptr) {
        return (UI_CMDBUFFSIZE - cp->rptr) + cp->wptr;
    } else {
        return cp->wptr - cp->rptr;
    }
}

// consume white space (0x20, space)
//  non_white_space = true, consume non-white space characters (not space)
bool cmdln_consume_white_space(uint32_t* rptr, bool non_white_space) {
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
bool cmdln_args_get_string(uint32_t rptr, uint32_t max_len, char* string) {
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
bool cmdln_args_get_hex(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
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
bool cmdln_args_get_bin(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
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
bool cmdln_args_get_dec(uint32_t* rptr, struct prompt_result* result, uint32_t* value) {
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

bool cmdln_args_find_flag_internal(char flag, command_var_t* arg) {
    uint32_t rptr = 0;
    char flag_c, dash_c, space_c;
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

// function for debugging the command line arguments parsers
//  shows all commands and all detected positions
bool cmdln_info(void) {
    // start and end point?
    printf("Input start: %d, end %d\r\n", cmdln.rptr, cmdln.wptr);
    // how many characters?
    printf("Input length: %d\r\n", cmdln_get_length_pointer(&cmdln));
    uint32_t i = 0;
    struct _command_info_t cp;
    cp.nextptr = 0;
    while (cmdln_find_next_command(&cp)) {
        printf("Command: %s, delimiter: %c\r\n", cp.command, cp.delimiter);
        uint32_t pos = 0;
        char str[9];
        while (cmdln_args_string_by_position(pos, 9, str)) {
            printf("String pos: %d, value: %s\r\n", pos, str);
            pos++;
        }
    }
}

// function for debugging the command line arguments parsers
//  shows all integers and all detected positions
bool cmdln_info_uint32(void) {
    // start and end point?
    printf("Input start: %d, end %d\r\n", cmdln.rptr, cmdln.wptr);
    // how many characters?
    printf("Input length: %d\r\n", cmdln_get_length_pointer(&cmdln));
    uint32_t i = 0;
    struct _command_info_t cp;
    cp.nextptr = 0;
    while (cmdln_find_next_command(&cp)) {
        printf("Command: %s, delimiter: %c\r\n", cp.command, cp.delimiter);
        uint32_t pos = 0;
        uint32_t value = 0;
        while (cmdln_args_uint32_by_position(pos, &value)) {
            printf("Integer pos: %d, value: %d\r\n", pos, value);
            pos++;
        }
    }
}