/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */

#include "charbuf.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#ifndef BUSPIRATE
#include <unistd.h>
#endif


/*
 * Create a charbuf on the heap and return it.
 */
struct charbuf* charbuf_create() {
	struct charbuf* b = malloc(sizeof(struct charbuf));
	if (b) {
		b->contents = NULL;
		b->len = 0;
		b->cap = 0;
		return b;
	} else {
		perror("Unable to allocate size for struct charbuf");
		exit(1);
	}
}

/*
 * Deletes the charbuf's contents, and the charbuf itself.
 */
void charbuf_free(struct charbuf* buf) {
	free(buf->contents);
	free(buf);
}

/*
 * Appends `what' to the charbuf, writing at most `len' bytes. Note that
 * if we use snprintf() to format a particular string, we have to subtract
 * 1 from the `len', to discard the null terminator character.
 */
void charbuf_append(struct charbuf* buf, const char* what, size_t len) {
	assert(what != NULL);

	// Prevent reallocing a lot by using some sort of geometric progression
	// by increasing the cap with len, then doubling it.
	if ((int)(buf->len + len) >= buf->cap) {
		buf->cap += len;
		buf->cap *= 2;
		// reallocate with twice the capacity
		buf->contents = realloc(buf->contents, buf->cap);
		if (buf->contents == NULL) {
			perror("Unable to realloc charbuf");
			exit(1);
		}
	}

	// copy 'what' to the target memory
	memcpy(buf->contents + buf->len, what, len);
	buf->len += len;
}

int charbuf_appendf(struct charbuf* buf, const char* fmt, ...) {
	assert(strlen(fmt) < CHARBUF_APPENDF_SIZE);
	// We use a fixed size buffer. We don't need to fmt a lot
	// of characters anyway.
	char buffer[CHARBUF_APPENDF_SIZE];
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	charbuf_append(buf, buffer, len);
	return len;
}

/*
 * Draws (writes) the charbuf to the screen.
 */
void charbuf_draw(struct charbuf* buf) {
#ifdef BUSPIRATE
	hx_io_write(1, buf->contents, buf->len);
#else
	if (write(STDOUT_FILENO, buf->contents, buf->len) == -1) {
		perror("Can't write charbuf");
		exit(1);
	}
#endif
}
