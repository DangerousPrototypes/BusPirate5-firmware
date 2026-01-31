/**
 * @file bp_linenoise.c
 * @brief Linenoise line editing adapted for Bus Pirate (RP2040/RP2350).
 * 
 * Based on linenoise by Salvatore Sanfilippo <antirez@gmail.com>
 * 
 * Original Copyright:
 * ------------------------------------------------------------------------
 * Copyright (c) 2010-2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ------------------------------------------------------------------------
 * 
 * Modifications for Bus Pirate:
 * - Removed malloc/free - uses static history buffer
 * - Removed POSIX I/O - uses callback functions
 * - Removed termios raw mode - USB CDC handles this
 * - Simplified for embedded use
 */

#include "bp_linenoise.h"
#include <string.h>
#include <stdio.h>

/*
 * =============================================================================
 * Linear Buffer Reader Implementation
 * =============================================================================
 */

// Global reader instance
bp_cmdln_reader_t bp_cmdln = {0};

void bp_cmdln_init_reader(const char *buf, size_t len) {
    bp_cmdln.buf = buf;
    bp_cmdln.len = len;
    bp_cmdln.pos = 0;
}

void bp_cmdln_reset(void) {
    bp_cmdln.pos = 0;
}

/**
 * Peek at character at absolute offset from read position.
 * 
 * This matches the semantics of the old cmdln_try_peek:
 * - cmdln_try_peek(0, &c) reads at current position
 * - cmdln_try_peek(5, &c) reads 5 ahead of current position
 * - Neither changes the position
 * 
 * Note: The old circular buffer used cmdln.rptr as "current position".
 * Here bp_cmdln.pos serves that role.
 */
bool bp_cmdln_try_peek(size_t i, char *c) {
    size_t target = bp_cmdln.pos + i;
    if (target >= bp_cmdln.len) {
        return false;
    }
    char ch = bp_cmdln.buf[target];
    if (ch == '\0') {
        return false;  // Treat null as end-of-input (compatibility)
    }
    *c = ch;
    return true;
}

/**
 * Advance read position by n characters.
 */
bool bp_cmdln_try_discard(size_t n) {
    bp_cmdln.pos += n;
    return true;
}

const char *bp_cmdln_current(void) {
    if (bp_cmdln.pos >= bp_cmdln.len) {
        return bp_cmdln.buf + bp_cmdln.len;  // Point to end
    }
    return bp_cmdln.buf + bp_cmdln.pos;
}

size_t bp_cmdln_remaining(void) {
    if (bp_cmdln.pos >= bp_cmdln.len) {
        return 0;
    }
    return bp_cmdln.len - bp_cmdln.pos;
}

/*
 * =============================================================================
 * Line Editing Implementation
 * =============================================================================
 */

// Key codes
enum {
    KEY_NULL = 0,
    CTRL_A = 1,
    CTRL_B = 2,
    CTRL_C = 3,
    CTRL_D = 4,
    CTRL_E = 5,
    CTRL_F = 6,
    CTRL_H = 8,
    TAB = 9,
    CTRL_K = 11,
    CTRL_L = 12,
    ENTER = 13,
    CTRL_N = 14,
    CTRL_P = 16,
    CTRL_T = 20,
    CTRL_U = 21,
    CTRL_W = 23,
    ESC = 27,
    BACKSPACE = 127
};

// Static history storage (circular buffer)
static char history_buf[BP_LINENOISE_HISTORY_MAX][BP_LINENOISE_MAX_LINE + 1];
static int history_head = 0;   // Next write position
static int history_count = 0;  // Number of entries (0..MAX)

// Get history entry by age (0 = newest, 1 = second newest, etc.)
static char* history_get(int age) {
    if (age < 0 || age >= history_count) return NULL;
    int idx = (history_head - 1 - age + BP_LINENOISE_HISTORY_MAX) % BP_LINENOISE_HISTORY_MAX;
    return history_buf[idx];
}

// Helper: write a string using callback
static void ln_write_str(bp_linenoise_state_t *state, const char *s) {
    state->write(s, strlen(s));
}

// Helper: write single character
static void ln_write_char(bp_linenoise_state_t *state, char c) {
    state->write(&c, 1);
}

// Helper: write escape sequence with number
static void ln_write_esc_num(bp_linenoise_state_t *state, const char *prefix, int n, char suffix) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%s%d%c", prefix, n, suffix);
    ln_write_str(state, buf);
}

// Calculate display width of string (handles ANSI escape sequences)
static size_t display_width(const char *s, size_t len) {
    size_t width = 0;
    size_t i = 0;
    
    while (i < len) {
        if (s[i] == '\x1b' && i + 1 < len && s[i + 1] == '[') {
            // ANSI escape sequence: skip until 'm' or end
            i += 2;  // Skip ESC [
            while (i < len && s[i] != 'm') {
                i++;
            }
            if (i < len) i++;  // Skip 'm'
        } else {
            // Regular character
            width++;
            i++;
        }
    }
    return width;
}

// Refresh the line on screen
static void refresh_line(bp_linenoise_state_t *state) {
    char seq[32];
    size_t pwidth = state->plen;
    size_t bufwidth = display_width(state->buf, state->len);
    size_t poswidth = display_width(state->buf, state->pos);
    
    // Calculate visible portion if line is too long
    char *buf = state->buf;
    size_t len = state->len;
    size_t pos = state->pos;
    
    // Scroll horizontally if cursor is past right edge
    while (pwidth + poswidth >= state->cols && len > 0) {
        buf++;
        len--;
        pos--;
        poswidth--;
        bufwidth--;
    }
    
    // Trim from right if still too long
    while (pwidth + bufwidth > state->cols && len > 0) {
        len--;
        bufwidth--;
    }
    
    // Move cursor to left edge
    ln_write_str(state, "\r");
    
    // Write prompt
    ln_write_str(state, state->prompt);
    
    // Write buffer content
    state->write(buf, len);
    
    // Erase to end of line
    ln_write_str(state, "\x1b[0K");
    
    // Move cursor to correct position
    snprintf(seq, sizeof(seq), "\r\x1b[%dC", (int)(poswidth + pwidth));
    ln_write_str(state, seq);
}

void bp_linenoise_init(bp_linenoise_state_t *state,
                       bp_ln_read_char_fn try_read,
                       bp_ln_read_char_blocking_fn read_blocking,
                       bp_ln_write_fn write,
                       size_t cols) {
    memset(state, 0, sizeof(*state));
    state->try_read = try_read;
    state->read_blocking = read_blocking;
    state->write = write;
    state->cols = cols;
    state->buflen = BP_LINENOISE_MAX_LINE;
    state->initialized = true;
}

void bp_linenoise_start(bp_linenoise_state_t *state, const char *prompt) {
    state->prompt = prompt;
    state->plen = display_width(prompt, strlen(prompt));
    state->pos = 0;
    state->len = 0;
    state->buf[0] = '\0';
    state->history_index = 0;
    state->history_save[0] = '\0';
    
    // Write prompt
    ln_write_str(state, prompt);
}

bp_linenoise_result_t bp_linenoise_feed(bp_linenoise_state_t *state) {
    char c;
    
    if (!state->try_read(&c)) {
        return BP_LN_CONTINUE;
    }
    
    switch (c) {
        case ENTER:
            return BP_LN_ENTER;
            
        case CTRL_C:
            return BP_LN_CTRL_C;
            
        case CTRL_D:
            if (state->len > 0) {
                bp_linenoise_edit_delete(state);
            } else {
                return BP_LN_CTRL_D;
            }
            break;
            
        case CTRL_B:  // Ctrl+B = backward (left) OR screen refresh in BP
            // In Bus Pirate, Ctrl+B (0x02) is used for screen refresh
            return BP_LN_REFRESH;
            
        case BACKSPACE:
        case CTRL_H:
            bp_linenoise_edit_backspace(state);
            break;
            
        case TAB:
            if (state->completion_callback) {
                state->completion_callback(state->buf, state->len);
            }
            // TODO: Implement tab completion
            break;
            
        case CTRL_A:  // Home
            bp_linenoise_edit_move_home(state);
            break;
            
        case CTRL_E:  // End
            bp_linenoise_edit_move_end(state);
            break;
            
        case CTRL_F:  // Forward (right)
            bp_linenoise_edit_move_right(state);
            break;
            
        case CTRL_K:  // Kill to end of line
            bp_linenoise_edit_delete_to_end(state);
            break;
            
        case CTRL_L:  // Clear screen
            bp_linenoise_clear_screen(state);
            break;
            
        case CTRL_N:  // Next in history
            bp_linenoise_edit_history_next(state);
            break;
            
        case CTRL_P:  // Previous in history
            bp_linenoise_edit_history_prev(state);
            break;
            
        case CTRL_T:  // Transpose characters
            bp_linenoise_edit_transpose(state);
            break;
            
        case CTRL_U:  // Delete whole line
            bp_linenoise_edit_delete_line(state);
            break;
            
        case CTRL_W:  // Delete previous word
            bp_linenoise_edit_delete_word(state);
            break;
            
        case ESC:  // Escape sequence
            {
                char seq[3];
                if (!state->read_blocking) break;
                
                state->read_blocking(&seq[0]);
                
                if (seq[0] == '[') {
                    state->read_blocking(&seq[1]);
                    
                    if (seq[1] >= '0' && seq[1] <= '9') {
                        // Extended escape (like Delete key: ESC[3~)
                        state->read_blocking(&seq[2]);
                        if (seq[2] == '~') {
                            switch (seq[1]) {
                                case '1':  // Home (some terminals)
                                    bp_linenoise_edit_move_home(state);
                                    break;
                                case '3':  // Delete
                                    bp_linenoise_edit_delete(state);
                                    break;
                                case '4':  // End (some terminals)
                                    bp_linenoise_edit_move_end(state);
                                    break;
                            }
                        }
                    } else {
                        switch (seq[1]) {
                            case 'A':  // Up
                                bp_linenoise_edit_history_prev(state);
                                break;
                            case 'B':  // Down
                                bp_linenoise_edit_history_next(state);
                                break;
                            case 'C':  // Right
                                bp_linenoise_edit_move_right(state);
                                break;
                            case 'D':  // Left
                                bp_linenoise_edit_move_left(state);
                                break;
                            case 'H':  // Home
                                bp_linenoise_edit_move_home(state);
                                break;
                            case 'F':  // End
                                bp_linenoise_edit_move_end(state);
                                break;
                        }
                    }
                } else if (seq[0] == 'O') {
                    // ESC O sequences (F1-F4, some Home/End)
                    state->read_blocking(&seq[1]);
                    switch (seq[1]) {
                        case 'H':  // Home
                            bp_linenoise_edit_move_home(state);
                            break;
                        case 'F':  // End
                            bp_linenoise_edit_move_end(state);
                            break;
                    }
                }
            }
            break;
            
        default:
            // Insert printable characters
            if (c >= ' ' && c <= '~') {
                bp_linenoise_edit_insert(state, c);
            }
            // TODO: Handle UTF-8 multi-byte sequences
            break;
    }
    
    return BP_LN_CONTINUE;
}

void bp_linenoise_stop(bp_linenoise_state_t *state) {
    ln_write_str(state, "\n");
    state->buf[state->len] = '\0';
}

const char *bp_linenoise_get_line(bp_linenoise_state_t *state) {
    return state->buf;
}

size_t bp_linenoise_get_len(bp_linenoise_state_t *state) {
    return state->len;
}

void bp_linenoise_edit_insert(bp_linenoise_state_t *state, char c) {
    if (state->len >= state->buflen) {
        return;  // Buffer full
    }
    
    if (state->pos == state->len) {
        // Append at end
        state->buf[state->pos] = c;
        state->pos++;
        state->len++;
        state->buf[state->len] = '\0';
        
        // Optimization: if at end and line fits, just write the char
        if (state->plen + state->len < state->cols) {
            ln_write_char(state, c);
        } else {
            refresh_line(state);
        }
    } else {
        // Insert in middle
        memmove(state->buf + state->pos + 1, 
                state->buf + state->pos, 
                state->len - state->pos);
        state->buf[state->pos] = c;
        state->pos++;
        state->len++;
        state->buf[state->len] = '\0';
        refresh_line(state);
    }
}

void bp_linenoise_edit_backspace(bp_linenoise_state_t *state) {
    if (state->pos == 0 || state->len == 0) {
        return;
    }
    
    memmove(state->buf + state->pos - 1,
            state->buf + state->pos,
            state->len - state->pos);
    state->pos--;
    state->len--;
    state->buf[state->len] = '\0';
    refresh_line(state);
}

void bp_linenoise_edit_delete(bp_linenoise_state_t *state) {
    if (state->len == 0 || state->pos >= state->len) {
        return;
    }
    
    memmove(state->buf + state->pos,
            state->buf + state->pos + 1,
            state->len - state->pos - 1);
    state->len--;
    state->buf[state->len] = '\0';
    refresh_line(state);
}

void bp_linenoise_edit_move_left(bp_linenoise_state_t *state) {
    if (state->pos > 0) {
        state->pos--;
        refresh_line(state);
    }
}

void bp_linenoise_edit_move_right(bp_linenoise_state_t *state) {
    if (state->pos < state->len) {
        state->pos++;
        refresh_line(state);
    }
}

void bp_linenoise_edit_move_home(bp_linenoise_state_t *state) {
    if (state->pos > 0) {
        state->pos = 0;
        refresh_line(state);
    }
}

void bp_linenoise_edit_move_end(bp_linenoise_state_t *state) {
    if (state->pos < state->len) {
        state->pos = state->len;
        refresh_line(state);
    }
}

void bp_linenoise_edit_history_prev(bp_linenoise_state_t *state) {
    if (history_count == 0) {
        return;
    }
    
    // Save current line if we're at history_index 0
    if (state->history_index == 0) {
        memcpy(state->history_save, state->buf, state->len + 1);
    }
    
    if (state->history_index < history_count) {
        state->history_index++;
        
        // Load from history (circular buffer, 0 = newest)
        char *hist = history_get(state->history_index - 1);
        if (hist) {
            strncpy(state->buf, hist, state->buflen);
            state->buf[state->buflen] = '\0';
            state->len = strlen(state->buf);
            state->pos = state->len;
            refresh_line(state);
        }
    }
}

void bp_linenoise_edit_history_next(bp_linenoise_state_t *state) {
    if (state->history_index > 0) {
        state->history_index--;
        
        if (state->history_index == 0) {
            // Restore saved line
            strncpy(state->buf, state->history_save, state->buflen);
        } else {
            char *hist = history_get(state->history_index - 1);
            if (hist) {
                strncpy(state->buf, hist, state->buflen);
            }
        }
        state->buf[state->buflen] = '\0';
        state->len = strlen(state->buf);
        state->pos = state->len;
        refresh_line(state);
    }
}

void bp_linenoise_edit_delete_line(bp_linenoise_state_t *state) {
    state->buf[0] = '\0';
    state->pos = 0;
    state->len = 0;
    refresh_line(state);
}

void bp_linenoise_edit_delete_to_end(bp_linenoise_state_t *state) {
    state->buf[state->pos] = '\0';
    state->len = state->pos;
    refresh_line(state);
}

void bp_linenoise_edit_delete_word(bp_linenoise_state_t *state) {
    size_t old_pos = state->pos;
    
    // Skip spaces backwards
    while (state->pos > 0 && state->buf[state->pos - 1] == ' ') {
        state->pos--;
    }
    
    // Skip non-spaces backwards
    while (state->pos > 0 && state->buf[state->pos - 1] != ' ') {
        state->pos--;
    }
    
    size_t diff = old_pos - state->pos;
    memmove(state->buf + state->pos, 
            state->buf + old_pos,
            state->len - old_pos + 1);
    state->len -= diff;
    refresh_line(state);
}

void bp_linenoise_edit_transpose(bp_linenoise_state_t *state) {
    if (state->pos > 0 && state->pos < state->len) {
        char tmp = state->buf[state->pos - 1];
        state->buf[state->pos - 1] = state->buf[state->pos];
        state->buf[state->pos] = tmp;
        if (state->pos < state->len) {
            state->pos++;
        }
        refresh_line(state);
    }
}

bool bp_linenoise_history_add(bp_linenoise_state_t *state, const char *line) {
    (void)state;  // Uses global history
    
    if (!line || !line[0]) {
        return false;  // Don't add empty lines
    }
    
    // Don't add duplicates (check newest entry)
    char *newest = history_get(0);
    if (newest && strcmp(newest, line) == 0) {
        return false;
    }
    
    // Write at head position (circular - overwrites oldest when full)
    strncpy(history_buf[history_head], line, BP_LINENOISE_MAX_LINE);
    history_buf[history_head][BP_LINENOISE_MAX_LINE] = '\0';
    
    // Advance head
    history_head = (history_head + 1) % BP_LINENOISE_HISTORY_MAX;
    
    // Update count (cap at max)
    if (history_count < BP_LINENOISE_HISTORY_MAX) {
        history_count++;
    }
    
    return true;
}

void bp_linenoise_history_clear(bp_linenoise_state_t *state) {
    (void)state;
    history_head = 0;
    history_count = 0;
}

void bp_linenoise_set_cols(bp_linenoise_state_t *state, size_t cols) {
    state->cols = cols;
}

void bp_linenoise_clear_screen(bp_linenoise_state_t *state) {
    ln_write_str(state, "\x1b[H\x1b[2J");
    refresh_line(state);
}

void bp_linenoise_set_completion(bp_linenoise_state_t *state,
                                  bp_ln_completion_fn callback) {
    state->completion_callback = callback;
}
