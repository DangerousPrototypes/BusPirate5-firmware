/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */

#include <stdlib.h> // size_t

#ifndef HX_CHARBUF_H
#define HX_CHARBUF_H

static const unsigned int CHARBUF_APPENDF_SIZE = 1024;

/*
 * This charbuf contains the character sequences to render the current
 * 'screen'. The charbuf is changed as a whole, then written to the screen
 * in one go to prevent 'flickering' in the terminal. The charbuf behaves
 * like a sort-of interface to a changeable array of characters.
 */
struct charbuf {
	char* contents;
	int len;        // actual length of what's in the buffer
	int cap;        // capacity
};

/*
 * Create a charbuf on the heap and return it.
 */
struct charbuf* charbuf_create();

/*
 * Deletes the charbuf's contents, and the charbuf itself.
 */
void charbuf_free(struct charbuf* buf);

/*
 * Appends `what' to the charbuf, writing exactly `len' bytes.
 */
void charbuf_append(struct charbuf* buf, const char* what, size_t len);

/*
 * Appends `what' to the charbuf, which can be a formatted string
 * processed by `vsnprintf'. If you know beforehand what size you
 * need to append to the charbuf, use charbuf_append instead.
 *
 * The amount of characters written by vsnprintf are returned,
 * excluding the zero terminator string.
 */
int charbuf_appendf(struct charbuf* buf, const char* what, ...);

/*
 * Draws (writes) the charbuf to the screen.
 */
void charbuf_draw(struct charbuf* buf);

#endif // HX_CHARBUF_H
