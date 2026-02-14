/* ln_cmdreader.h -- Linear buffer reader for command/syntax parsing.
 *
 * This is NOT part of the linenoise library. It provides a peek/discard
 * interface over the finished edit buffer, used by the Bus Pirate syntax
 * parser and command processor.
 *
 * Replaces what was previously a circular FIFO with a simple linear scan.
 *
 * Copyright (c) Bus Pirate project contributors.
 * SPDX-License-Identifier: MIT
 */

#ifndef LN_CMDREADER_H
#define LN_CMDREADER_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Linear buffer reader state.
 * @details Points to the current line being processed (from linenoise).
 *          Provides cmdln_try_peek/discard compatible interface.
 */
typedef struct {
    const char *buf;    ///< Pointer to line buffer
    size_t len;         ///< Total length of content
    size_t pos;         ///< Current read position
} ln_cmdreader_t;

/**
 * @brief Global command line reader instance.
 */
extern ln_cmdreader_t ln_cmdln;

/**
 * @brief Initialize reader with a line buffer.
 * @param buf   Line buffer (null-terminated)
 * @param len   Length of content
 */
void ln_cmdln_init(const char *buf, size_t len);

/**
 * @brief Reset reader position to start.
 */
void ln_cmdln_reset(void);

/**
 * @brief Try to peek at character at offset from current position.
 * @param offset  Offset from current position
 * @param[out] c  Character at position
 * @return true if character available, false if past end
 */
bool ln_cmdln_try_peek(size_t offset, char *c);

/**
 * @brief Discard n characters by advancing read position.
 * @param n  Number of characters to discard
 * @return true (always succeeds, may advance past end)
 */
bool ln_cmdln_try_discard(size_t n);

/**
 * @brief Get pointer to current position in buffer.
 * @return Pointer to buf[pos]
 */
const char *ln_cmdln_current(void);

/**
 * @brief Get remaining length from current position.
 * @return Remaining characters
 */
size_t ln_cmdln_remaining(void);

/**
 * @brief Advance reader position to a specific pointer within the buffer.
 * @param p  Pointer within buf (must be >= buf and <= buf+len)
 */
void ln_cmdln_advance_to(const char *p);

/*
 * =============================================================================
 * Compatibility Macros
 * =============================================================================
 * Map the legacy cmdln_try_* names to the ln_cmdln_* functions.
 * All callers use these names; the underlying implementation is ln_cmdreader.
 */
#define cmdln_try_peek(i, c)    ln_cmdln_try_peek((i), (c))
#define cmdln_try_discard(i)    ln_cmdln_try_discard((i))
#define cmdln_try_remove(c)     (ln_cmdln_try_peek(0, (c)) ? (ln_cmdln_try_discard(1), true) : false)

#endif /* LN_CMDREADER_H */
