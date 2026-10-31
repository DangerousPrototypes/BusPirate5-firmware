/* ln_cmdreader.c -- Linear buffer reader for command/syntax parsing.
 *
 * This is NOT part of the linenoise library. See ln_cmdreader.h for details.
 *
 * Copyright (c) Bus Pirate project contributors.
 * SPDX-License-Identifier: MIT
 */

#include "ln_cmdreader.h"
#include <string.h>

/* Global reader instance. */
ln_cmdreader_t ln_cmdln = {0};

void ln_cmdln_init(const char *buf, size_t len) {
    ln_cmdln.buf = buf;
    ln_cmdln.len = len;
    ln_cmdln.pos = 0;
}

void ln_cmdln_reset(void) {
    ln_cmdln.pos = 0;
}

bool ln_cmdln_try_peek(size_t offset, char *c) {
    size_t target = ln_cmdln.pos + offset;
    if (target >= ln_cmdln.len) {
        return false;
    }
    char ch = ln_cmdln.buf[target];
    if (ch == '\0') {
        return false;  /* Treat null as end-of-input (compatibility). */
    }
    *c = ch;
    return true;
}

bool ln_cmdln_try_discard(size_t n) {
    ln_cmdln.pos += n;
    return true;
}

const char *ln_cmdln_current(void) {
    if (ln_cmdln.pos >= ln_cmdln.len) {
        return ln_cmdln.buf + ln_cmdln.len;  /* Point to end. */
    }
    return ln_cmdln.buf + ln_cmdln.pos;
}

size_t ln_cmdln_remaining(void) {
    if (ln_cmdln.pos >= ln_cmdln.len) {
        return 0;
    }
    return ln_cmdln.len - ln_cmdln.pos;
}

void ln_cmdln_advance_to(const char *p) {
    if (p >= ln_cmdln.buf && p <= ln_cmdln.buf + ln_cmdln.len) {
        ln_cmdln.pos = (size_t)(p - ln_cmdln.buf);
    }
}
