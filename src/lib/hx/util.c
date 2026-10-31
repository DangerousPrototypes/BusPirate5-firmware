/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int hex2bin(const char* s) {
	int ret=0;
	for(int i = 0; i < 2; i++) {
		char c = *s++;
		int n=0;
		if( '0' <= c && c <= '9')  {
			n = c-'0';
		} else if ('a' <= c && c <= 'f') {
			n = 10 + c - 'a';
		} else if ('A' <= c && c <= 'F') {
			n = 10 + c - 'A';
		}
		ret = n + ret*16;
	}
	return ret;
}

bool is_pos_num(const char* s) {
	for (const char* ptr = s; *ptr; ptr++) {
		if (!isdigit(*ptr)) {
			return false;
		}
	}
	return true;
}

bool is_hex(const char* s) {
	const char* ptr = s;
	while(*++ptr) {
		if (!isxdigit(*ptr)) {
			return false;
		}
	}
	return true;
}

int hex2int(const char* s) {
	char* endptr;
	intmax_t x = strtoimax(s, &endptr, 16);
	if (errno == ERANGE) {
		return 0;
	}

	return x;
}

inline int clampi(int i, int min, int max) {
	if (i < min) {
		return min;
	}
	if (i > max) {
		return max;
	}
	return i;
}

int str2int(const char* s, int min, int max, int def) {
	char* endptr;
	errno = 0;
	intmax_t x = strtoimax(s, &endptr, 10);
	if (errno  == ERANGE) {
		return def;
	}
	if (x < min || x > max) {
		return def;
	}
	return x;
}

/*
 * Reads keypresses from stdin, and processes them accordingly. Escape sequences
 * will be read properly as well (e.g. DEL will be the bytes 0x1b, 0x5b, 0x33, 0x7e).
 * The returned integer will contain either one of the enum values, or the key pressed.
 *
 * read_key() will only return the correct key code, or -1 when anything fails.
 */
void read_key_unget(int key) {
	vt100_key_unget(&hx_key_state, key);
}

int read_key() {
	/* Use the shared vt100_keys decoder.
	 * Apply hx's app-level Ctrl remaps on top. */
	int k = vt100_key_read(&hx_key_state);
	switch (k) {
	case VT100_KEY_CTRL_H:  return VT100_KEY_BACKSPACE;
	case VT100_KEY_CTRL_B:  return VT100_KEY_PAGEUP;
	case VT100_KEY_CTRL_F:  return VT100_KEY_PAGEDOWN;
	default:                return k;
	}
}

bool get_window_size(int* rows, int* cols) {
	return hx_get_window_size(rows, cols);
}

void term_state_save() {
	/* No-op on embedded — terminal is already in alt-screen */
}

void term_state_restore() {
	/* No-op on embedded */
}

void enable_raw_mode() {
	/* No-op on embedded — terminal is always raw */
}

void disable_raw_mode() {
	/* No-op on embedded */
}

void clear_screen() {
	// clear the colors, move the cursor up-left, clear the screen.
	char stuff[80];
	int bw = snprintf(stuff, 80, "\x1b[0m\x1b[H\x1b[2J");
	hx_io_write(1, stuff, bw);
}
