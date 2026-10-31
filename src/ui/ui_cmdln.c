/**
 * @file ui_cmdln.c
 * @brief Command line argument parsing implementation.
 * @details Provides command dispatch infrastructure using linenoise linear buffer:
 *          - Command chaining with delimiters (; || &&)
 *          - Command name extraction by position
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_cmdln.h"
#include "lib/bp_linenoise/ln_cmdreader.h"

struct _command_info_t command_info; // the current command and position in the buffer

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
            (*rptr)++;
        } else {
            break;
        }
    }
    return true;
}

// internal function to copy string from start position to next space or end of buffer
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

