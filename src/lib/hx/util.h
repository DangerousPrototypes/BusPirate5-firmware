/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */

#ifndef HX_UTIL_H
#define HX_UTIL_H

#include <stdbool.h>

// Key codes come from the shared vt100_keys library.
#include "lib/vt100_keys/vt100_keys.h"

// Errors which may be returned by parse_search_string.
enum parse_errors {
	PARSE_SUCCESS,
	PARSE_INCOMPLETE_BACKSLASH,  // "...\"
	PARSE_INCOMPLETE_HEX,        // "...\x" or "...\xA"
	PARSE_INVALID_HEX,           // "...\xXY..." and X or Y not in [a-zA-Z0-9]
	PARSE_INVALID_ESCAPE,        // "...\a..." and a is not '\' or 'x'
};

/*
 * Saves the current terminal state (the contents) so it can be restored
 * later, when hx exits.
 */
void term_state_save();

/*
 * This restores the previously saved terminal contents.
 */
void term_state_restore();

void enable_raw_mode();
void disable_raw_mode();
void clear_screen();
int  read_key();
void read_key_unget(int key);
int  hex2bin(const char* s);
bool get_window_size(int* rows, int* cols);

/*
 * Returns true when the given char can be successfully parsed as a positive
 * integer, or return false if otherwise.
 */
bool is_pos_num(const char* s);

/*
 * Returns true when the given string is a valid hexadecimal string, or false
 * if othwerise.
 */
bool is_hex(const char* s);

int hex2int(const char* s);

/*
 * Clamps the given integer i to the given min or max.
 */
int clampi(int i, int min, int max);

/*
 * Parses a string to an integer and returns it. In case of errors, the default
 * `def' will be returned.
 */
int str2int(const char* s, int min, int max, int def);

#endif // HX_UTIL_H
