/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2023, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward n chars
 *
 * CUB (CUrsor Backward)
 *    Sequence: ESC [ n D
 *    Effect: moves cursor backward n chars
 *
 * The following is used to get the terminal width if getting
 * the width with the TIOCGWINSZ ioctl fails
 *
 * DSR (Device Status Report)
 *    Sequence: ESC [ 6 n
 *    Effect: reports the current cusor position as ESC [ n ; m R
 *            where n is the row and m is the column
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * When linenoiseClearScreen() is called, two additional escape sequences
 * are used in order to clear the screen and position the cursor at home
 * position.
 *
 * CUP (Cursor position)
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED (Erase display)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#ifndef BP_EMBEDDED
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "linenoise.h"

#ifndef BP_EMBEDDED
#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
#endif

static linenoiseCompletionCallback *completionCallback = NULL;
static linenoiseHintsCallback *hintsCallback = NULL;
static linenoiseFreeHintsCallback *freeHintsCallback = NULL;
#ifndef BP_EMBEDDED
static char *linenoiseNoTTY(void);
#endif
static void refreshLineWithCompletion(struct linenoiseState *ls, linenoiseCompletions *lc, int flags);
static void refreshLineWithFlags(struct linenoiseState *l, int flags);

#ifdef BP_EMBEDDED
/* Static history storage for embedded targets (no malloc). */
static char history_storage[BP_LINENOISE_HISTORY_MAX][BP_LINENOISE_MAX_LINE + 1];
static char *history_ptrs[BP_LINENOISE_HISTORY_MAX];
static int history_max_len = BP_LINENOISE_HISTORY_MAX;
static int history_len = 0;
static char **history = history_ptrs;
static int history_inited = 0;
static int maskmode = 0;
static int mlmode = 0;
#else
static char *unsupported_term[] = {"dumb","cons25","emacs",NULL};
static struct termios orig_termios; /* In order to restore at exit.*/
static int maskmode = 0; /* Show "***" instead of input. For passwords. */
static int rawmode = 0; /* For atexit() function to check if restore is needed*/
static int mlmode = 0;  /* Multi line mode. Default is single line. */
static int atexit_registered = 0; /* Register atexit just 1 time. */
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;
#endif

/* ======================== I/O abstraction layer =========================== */
#ifdef BP_EMBEDDED
static int lnWrite(struct linenoiseState *l, const char *s, int len) {
    l->write_fn(s, (size_t)len);
    return len;
}
static int lnRead(struct linenoiseState *l, char *c, int n) {
    (void)n;
    if (l->try_read(c)) return 1;
    return 0;
}
static int lnReadBlocking(struct linenoiseState *l, char *c, int n) {
    (void)n;
    if (l->read_blocking) { l->read_blocking(c); return 1; }
    return -1;
}
#else
#define lnWrite(l, s, len) write((l)->ofd, (s), (len))
#define lnRead(l, c, n)    read((l)->ifd, (c), (n))
#define lnReadBlocking(l, c, n) read((l)->ifd, (c), (n))
#endif

/* =========================== UTF-8 support ================================ */

/* Return the number of bytes that compose the UTF-8 character starting at
 * 'c'. This function assumes a valid UTF-8 encoding and handles the four
 * standard byte patterns:
 *   0xxxxxxx -> 1 byte (ASCII)
 *   110xxxxx -> 2 bytes
 *   1110xxxx -> 3 bytes
 *   11110xxx -> 4 bytes */
static int utf8ByteLen(char c) {
    unsigned char uc = (unsigned char)c;
    if ((uc & 0x80) == 0)    return 1;   /* 0xxxxxxx: ASCII */
    if ((uc & 0xE0) == 0xC0) return 2;   /* 110xxxxx: 2-byte seq */
    if ((uc & 0xF0) == 0xE0) return 3;   /* 1110xxxx: 3-byte seq */
    if ((uc & 0xF8) == 0xF0) return 4;   /* 11110xxx: 4-byte seq */
    return 1; /* Fallback for invalid encoding, treat as single byte. */
}

/* Decode a UTF-8 sequence starting at 's' into a Unicode codepoint.
 * Returns the codepoint value. Assumes valid UTF-8 encoding. */
static uint32_t utf8DecodeChar(const char *s, size_t *len) {
    unsigned char *p = (unsigned char *)s;
    uint32_t cp;

    if ((*p & 0x80) == 0) {
        *len = 1;
        return *p;
    } else if ((*p & 0xE0) == 0xC0) {
        *len = 2;
        cp = (*p & 0x1F) << 6;
        cp |= (p[1] & 0x3F);
        return cp;
    } else if ((*p & 0xF0) == 0xE0) {
        *len = 3;
        cp = (*p & 0x0F) << 12;
        cp |= (p[1] & 0x3F) << 6;
        cp |= (p[2] & 0x3F);
        return cp;
    } else if ((*p & 0xF8) == 0xF0) {
        *len = 4;
        cp = (*p & 0x07) << 18;
        cp |= (p[1] & 0x3F) << 12;
        cp |= (p[2] & 0x3F) << 6;
        cp |= (p[3] & 0x3F);
        return cp;
    }
    *len = 1;
    return *p; /* Fallback for invalid sequences. */
}

/* Check if codepoint is a variation selector (emoji style modifiers). */
static int isVariationSelector(uint32_t cp) {
    return cp == 0xFE0E || cp == 0xFE0F;  /* Text/emoji style */
}

/* Check if codepoint is a skin tone modifier. */
static int isSkinToneModifier(uint32_t cp) {
    return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

/* Check if codepoint is Zero Width Joiner. */
static int isZWJ(uint32_t cp) {
    return cp == 0x200D;
}

/* Check if codepoint is a Regional Indicator (for flag emoji). */
static int isRegionalIndicator(uint32_t cp) {
    return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

/* Check if codepoint is a combining mark or other zero-width character. */
static int isCombiningMark(uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||   /* Combining Diacriticals */
           (cp >= 0x1AB0 && cp <= 0x1AFF) ||   /* Combining Diacriticals Extended */
           (cp >= 0x1DC0 && cp <= 0x1DFF) ||   /* Combining Diacriticals Supplement */
           (cp >= 0x20D0 && cp <= 0x20FF) ||   /* Combining Diacriticals for Symbols */
           (cp >= 0xFE20 && cp <= 0xFE2F);     /* Combining Half Marks */
}

/* Check if codepoint extends the previous character (doesn't start a new grapheme). */
static int isGraphemeExtend(uint32_t cp) {
    return isVariationSelector(cp) || isSkinToneModifier(cp) ||
           isZWJ(cp) || isCombiningMark(cp);
}

/* Decode the UTF-8 codepoint ending at position 'pos' (exclusive) and
 * return its value. Also sets *cplen to the byte length of the codepoint. */
static uint32_t utf8DecodePrev(const char *buf, size_t pos, size_t *cplen) {
    if (pos == 0) {
        *cplen = 0;
        return 0;
    }
    /* Scan backwards to find the start byte. */
    size_t i = pos;
    do {
        i--;
    } while (i > 0 && (pos - i) < 4 && ((unsigned char)buf[i] & 0xC0) == 0x80);
    *cplen = pos - i;
    size_t dummy;
    return utf8DecodeChar(buf + i, &dummy);
}

/* Given a buffer and a position, return the byte length of the grapheme
 * cluster before that position. A grapheme cluster includes:
 * - The base character
 * - Any following variation selectors, skin tone modifiers
 * - ZWJ sequences (emoji joined by Zero Width Joiner)
 * - Regional indicator pairs (flag emoji) */
static size_t utf8PrevCharLen(const char *buf, size_t pos) {
    if (pos == 0) return 0;

    size_t total = 0;
    size_t curpos = pos;

    /* First, get the last codepoint. */
    size_t cplen;
    uint32_t cp = utf8DecodePrev(buf, curpos, &cplen);
    if (cplen == 0) return 0;
    total += cplen;
    curpos -= cplen;

    /* If we're at an extending character, we need to find what it extends.
     * Keep going back through the grapheme cluster. */
    while (curpos > 0) {
        size_t prevlen;
        uint32_t prevcp = utf8DecodePrev(buf, curpos, &prevlen);
        if (prevlen == 0) break;

        if (isZWJ(prevcp)) {
            /* ZWJ joins two emoji. Include the ZWJ and continue to get
             * the preceding character. */
            total += prevlen;
            curpos -= prevlen;
            /* Now get the character before ZWJ. */
            prevcp = utf8DecodePrev(buf, curpos, &prevlen);
            if (prevlen == 0) break;
            total += prevlen;
            curpos -= prevlen;
            cp = prevcp;
            continue;  /* Check if there's more extending before this. */
        } else if (isGraphemeExtend(cp)) {
            /* Current cp is an extending character; include previous. */
            total += prevlen;
            curpos -= prevlen;
            cp = prevcp;
            continue;
        } else if (isRegionalIndicator(cp) && isRegionalIndicator(prevcp)) {
            /* Two regional indicators form a flag. But we need to be careful:
             * flags are always pairs, so only join if we're at an even boundary.
             * For simplicity, just join one pair. */
            total += prevlen;
            curpos -= prevlen;
            break;
        } else {
            /* No more extending; we've found the start of the cluster. */
            break;
        }
    }

    return total;
}

/* Given a buffer, position and total length, return the byte length of the
 * grapheme cluster at the current position. */
static size_t utf8NextCharLen(const char *buf, size_t pos, size_t len) {
    if (pos >= len) return 0;

    size_t total = 0;
    size_t curpos = pos;

    /* Get the first codepoint. */
    size_t cplen;
    uint32_t cp = utf8DecodeChar(buf + curpos, &cplen);
    total += cplen;
    curpos += cplen;

    int isRI = isRegionalIndicator(cp);

    /* Consume any extending characters that follow. */
    while (curpos < len) {
        size_t nextlen;
        uint32_t nextcp = utf8DecodeChar(buf + curpos, &nextlen);

        if (isZWJ(nextcp) && curpos + nextlen < len) {
            /* ZWJ: include it and the following character. */
            total += nextlen;
            curpos += nextlen;
            /* Get the character after ZWJ. */
            nextcp = utf8DecodeChar(buf + curpos, &nextlen);
            total += nextlen;
            curpos += nextlen;
            continue;  /* Check for more extending after the joined char. */
        } else if (isGraphemeExtend(nextcp)) {
            /* Variation selector, skin tone, combining mark, etc. */
            total += nextlen;
            curpos += nextlen;
            continue;
        } else if (isRI && isRegionalIndicator(nextcp)) {
            /* Second regional indicator for a flag pair. */
            total += nextlen;
            curpos += nextlen;
            isRI = 0;  /* Only pair once. */
            continue;
        } else {
            break;
        }
    }

    return total;
}

/* Return the display width of a Unicode codepoint. This is a heuristic
 * that works for most common cases:
 * - Control chars and zero-width: 0 columns
 * - Grapheme-extending chars (VS, skin tone, ZWJ): 0 columns
 * - ASCII printable: 1 column
 * - Wide chars (CJK, emoji, fullwidth): 2 columns
 * - Everything else: 1 column
 *
 * This is not a full wcwidth() implementation, but a minimal heuristic
 * that handles emoji and CJK characters reasonably well. */
static int utf8CharWidth(uint32_t cp) {
    /* Control characters and combining marks: zero width. */
    if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;
    if (isCombiningMark(cp)) return 0;

    /* Grapheme-extending characters: zero width.
     * These modify the preceding character rather than taking space. */
    if (isVariationSelector(cp)) return 0;
    if (isSkinToneModifier(cp)) return 0;
    if (isZWJ(cp)) return 0;

    /* Wide character ranges - these display as 2 columns:
     * - CJK Unified Ideographs and Extensions
     * - Fullwidth forms
     * - Various emoji ranges */
    if (cp >= 0x1100 &&
        (cp <= 0x115F ||                      /* Hangul Jamo */
         cp == 0x2329 || cp == 0x232A ||      /* Angle brackets */
         (cp >= 0x231A && cp <= 0x231B) ||    /* Watch, Hourglass */
         (cp >= 0x23E9 && cp <= 0x23F3) ||    /* Various symbols */
         (cp >= 0x23F8 && cp <= 0x23FA) ||    /* Various symbols */
         (cp >= 0x25AA && cp <= 0x25AB) ||    /* Small squares */
         (cp >= 0x25B6 && cp <= 0x25C0) ||    /* Play/reverse buttons */
         (cp >= 0x25FB && cp <= 0x25FE) ||    /* Squares */
         (cp >= 0x2600 && cp <= 0x26FF) ||    /* Misc Symbols (sun, cloud, etc) */
         (cp >= 0x2700 && cp <= 0x27BF) ||    /* Dingbats (❤, ✂, etc) */
         (cp >= 0x2934 && cp <= 0x2935) ||    /* Arrows */
         (cp >= 0x2B05 && cp <= 0x2B07) ||    /* Arrows */
         (cp >= 0x2B1B && cp <= 0x2B1C) ||    /* Squares */
         cp == 0x2B50 || cp == 0x2B55 ||      /* Star, circle */
         (cp >= 0x2E80 && cp <= 0xA4CF &&
          cp != 0x303F) ||                    /* CJK ... Yi */
         (cp >= 0xAC00 && cp <= 0xD7A3) ||    /* Hangul Syllables */
         (cp >= 0xF900 && cp <= 0xFAFF) ||    /* CJK Compatibility Ideographs */
         (cp >= 0xFE10 && cp <= 0xFE1F) ||    /* Vertical forms */
         (cp >= 0xFE30 && cp <= 0xFE6F) ||    /* CJK Compatibility Forms */
         (cp >= 0xFF00 && cp <= 0xFF60) ||    /* Fullwidth Forms */
         (cp >= 0xFFE0 && cp <= 0xFFE6) ||    /* Fullwidth Signs */
         (cp >= 0x1F1E6 && cp <= 0x1F1FF) ||  /* Regional Indicators (flags) */
         (cp >= 0x1F300 && cp <= 0x1F64F) ||  /* Misc Symbols and Emoticons */
         (cp >= 0x1F680 && cp <= 0x1F6FF) ||  /* Transport and Map Symbols */
         (cp >= 0x1F900 && cp <= 0x1F9FF) ||  /* Supplemental Symbols */
         (cp >= 0x1FA00 && cp <= 0x1FAFF) ||  /* Chess, Extended-A */
         (cp >= 0x20000 && cp <= 0x2FFFF)))   /* CJK Extension B and beyond */
        return 2;

    return 1; /* Default: single width */
}

/* Calculate the display width of a UTF-8 string of 'len' bytes.
 * This is used for cursor positioning in the terminal.
 * Handles grapheme clusters: characters joined by ZWJ contribute 0 width
 * after the first character in the sequence.
 * Skips ANSI escape sequences (ESC [ ... final_byte) so that color codes
 * in prompts don't inflate the reported width. */
static size_t utf8StrWidth(const char *s, size_t len) {
    size_t width = 0;
    size_t i = 0;
    int after_zwj = 0;  /* Track if previous char was ZWJ */

    while (i < len) {
        /* Skip ANSI escape sequences: ESC [ <params> <final byte 0x40-0x7E>
         * Also handles ESC ( and other two-byte sequences. */
        if ((unsigned char)s[i] == 0x1b && i + 1 < len) {
            i++; /* skip ESC */
            if (s[i] == '[') {
                /* CSI sequence: skip until 0x40-0x7E */
                i++;
                while (i < len && (unsigned char)s[i] < 0x40) i++;
                if (i < len) i++; /* skip final byte */
            } else {
                /* Two-byte escape (e.g. ESC ( B): skip one more byte */
                i++;
            }
            continue;
        }

        size_t clen;
        uint32_t cp = utf8DecodeChar(s + i, &clen);

        if (after_zwj) {
            /* Character after ZWJ: don't add width, it's joined.
             * But do check for extending chars after it. */
            after_zwj = 0;
        } else {
            width += utf8CharWidth(cp);
        }

        /* Check if this is a ZWJ - next char will be joined. */
        if (isZWJ(cp)) {
            after_zwj = 1;
        }

        i += clen;
    }
    return width;
}

/* Return the display width of a single UTF-8 character at position 's'. */
static int utf8SingleCharWidth(const char *s, size_t len) {
    if (len == 0) return 0;
    size_t clen;
    uint32_t cp = utf8DecodeChar(s, &clen);
    return utf8CharWidth(cp);
}

enum KEY_ACTION{
	KEY_NULL = 0,	    /* NULL */
	CTRL_A = 1,         /* Ctrl+a */
	CTRL_B = 2,         /* Ctrl-b */
	CTRL_C = 3,         /* Ctrl-c */
	CTRL_D = 4,         /* Ctrl-d */
	CTRL_E = 5,         /* Ctrl-e */
	CTRL_F = 6,         /* Ctrl-f */
	CTRL_H = 8,         /* Ctrl-h */
	TAB = 9,            /* Tab */
	CTRL_K = 11,        /* Ctrl+k */
	CTRL_L = 12,        /* Ctrl+l */
	ENTER = 13,         /* Enter */
	CTRL_N = 14,        /* Ctrl-n */
	CTRL_P = 16,        /* Ctrl-p */
	CTRL_T = 20,        /* Ctrl-t */
	CTRL_U = 21,        /* Ctrl+u */
	CTRL_W = 23,        /* Ctrl+w */
	ESC = 27,           /* Escape */
	BACKSPACE =  127    /* Backspace */
};

#ifndef BP_EMBEDDED
static void linenoiseAtExit(void);
#endif
int linenoiseHistoryAdd(const char *line);
#define REFRESH_CLEAN (1<<0)    // Clean the old prompt from the screen
#define REFRESH_WRITE (1<<1)    // Rewrite the prompt on the screen.
#define REFRESH_ALL (REFRESH_CLEAN|REFRESH_WRITE) // Do both.
static void refreshLine(struct linenoiseState *l);

/* Debugging macro. */
#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp/lndebug.txt","a"); \
            fprintf(lndebug_fp, \
            "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
            (int)l->len,(int)l->pos,(int)l->oldpos,plen,rows,rpos, \
            (int)l->oldrows,old_rows); \
        } \
        fprintf(lndebug_fp, ", " __VA_ARGS__); \
        fflush(lndebug_fp); \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif

/* ======================= Low level terminal handling ====================== */

/* Enable "mask mode". When it is enabled, instead of the input that
 * the user is typing, the terminal will just display a corresponding
 * number of asterisks, like "****". This is useful for passwords and other
 * secrets that should not be displayed. */
void linenoiseMaskModeEnable(void) {
    maskmode = 1;
}

/* Disable mask mode. */
void linenoiseMaskModeDisable(void) {
    maskmode = 0;
}

/* Set if to use or not the multi line mode. */
void linenoiseSetMultiLine(int ml) {
    mlmode = ml;
}

#ifndef BP_EMBEDDED
/* Return true if the terminal name is in the list of terminals we know are
 * not able to understand basic escape sequences. */
static int isUnsupportedTerm(void) {
    char *term = getenv("TERM");
    int j;

    if (term == NULL) return 0;
    for (j = 0; unsupported_term[j]; j++)
        if (!strcasecmp(term,unsupported_term[j])) return 1;
    return 0;
}

/* Raw mode: 1960 magic shit. */
static int enableRawMode(int fd) {
    struct termios raw;

    /* Test mode: when LINENOISE_ASSUME_TTY is set, skip terminal setup.
     * This allows testing via pipes without a real terminal. */
    if (getenv("LINENOISE_ASSUME_TTY")) {
        rawmode = 1;
        return 0;
    }

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

static void disableRawMode(int fd) {
    /* Test mode: nothing to restore. */
    if (getenv("LINENOISE_ASSUME_TTY")) {
        rawmode = 0;
        return;
    }
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd,TCSAFLUSH,&orig_termios) != -1)
        rawmode = 0;
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int getCursorPosition(int ifd, int ofd) {
    char buf[32];
    int cols, rows;
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",&rows,&cols) != 2) return -1;
    return cols;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
static int getColumns(int ifd, int ofd) {
    struct winsize ws;

    /* Test mode: use LINENOISE_COLS env var for fixed width. */
    char *cols_env = getenv("LINENOISE_COLS");
    if (cols_env) return atoi(cols_env);

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int start, cols;

        /* Get the initial position so we can restore it later. */
        start = getCursorPosition(ifd,ofd);
        if (start == -1) goto failed;

        /* Go to right margin and get position. */
        if (write(ofd,"\x1b[999C",6) != 6) goto failed;
        cols = getCursorPosition(ifd,ofd);
        if (cols == -1) goto failed;

        /* Restore position. */
        if (cols > start) {
            char seq[32];
            snprintf(seq,32,"\x1b[%dD",cols-start);
            if (write(ofd,seq,strlen(seq)) == -1) {
                /* Can't recover... */
            }
        }
        return cols;
    } else {
        return ws.ws_col;
    }

failed:
    return 80;
}
#endif /* !BP_EMBEDDED */

/* Clear the screen. Used to handle ctrl+l */
void linenoiseClearScreen(void) {
#ifdef BP_EMBEDDED
    /* No-op: embedded targets use linenoiseState callbacks. */
#else
    if (write(STDOUT_FILENO,"\x1b[H\x1b[2J",7) <= 0) {
        /* nothing to do, just to avoid warning. */
    }
#endif
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void linenoiseBeep(void) {
#ifndef BP_EMBEDDED
    fprintf(stderr, "\x7");
    fflush(stderr);
#endif
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void freeCompletions(linenoiseCompletions *lc) {
#ifdef BP_EMBEDDED
    /* Static array, nothing to free. Just reset count. */
    lc->len = 0;
#else
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    if (lc->cvec != NULL)
        free(lc->cvec);
#endif
}

/* Called by completeLine() and linenoiseShow() to render the current
 * edited line with the proposed completion. If the current completion table
 * is already available, it is passed as second argument, otherwise the
 * function will use the callback to obtain it.
 *
 * Flags are the same as refreshLine*(), that is REFRESH_* macros. */
static void refreshLineWithCompletion(struct linenoiseState *ls, linenoiseCompletions *lc, int flags) {
    /* Obtain the table of completions if the caller didn't provide one. */
    linenoiseCompletions ctable = { 0 };
    if (lc == NULL) {
        completionCallback(ls->buf,&ctable);
        lc = &ctable;
    }

    /* Show the edited line with completion if possible, or just refresh. */
    if (ls->completion_idx < lc->len) {
#ifdef BP_EMBEDDED
        /* Static buffer: copy the completion string temporarily. */
        char saved_buf[BP_LINENOISE_MAX_LINE + 1];
        size_t saved_len = ls->len;
        size_t saved_pos = ls->pos;
        memcpy(saved_buf, ls->buf, ls->len + 1);
        strncpy(ls->buf, lc->cvec[ls->completion_idx], ls->buflen);
        ls->buf[ls->buflen] = '\0';
        ls->len = ls->pos = strlen(ls->buf);
        refreshLineWithFlags(ls,flags);
        memcpy(ls->buf, saved_buf, saved_len + 1);
        ls->len = saved_len;
        ls->pos = saved_pos;
#else
        struct linenoiseState saved = *ls;
        ls->len = ls->pos = strlen(lc->cvec[ls->completion_idx]);
        ls->buf = lc->cvec[ls->completion_idx];
        refreshLineWithFlags(ls,flags);
        ls->len = saved.len;
        ls->pos = saved.pos;
        ls->buf = saved.buf;
#endif
    } else {
        refreshLineWithFlags(ls,flags);
    }

    /* Free the completions table if needed. */
    if (lc != &ctable) freeCompletions(&ctable);
}

/* This is an helper function for linenoiseEdit*() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition.
 *
 * If the function returns non-zero, the caller should handle the
 * returned value as a byte read from the standard input, and process
 * it as usually: this basically means that the function may return a byte
 * read from the termianl but not processed. Otherwise, if zero is returned,
 * the input was consumed by the completeLine() function to navigate the
 * possible completions, and the caller should read for the next characters
 * from stdin. */
static int completeLine(struct linenoiseState *ls, int keypressed) {
    linenoiseCompletions lc = { 0 };
    int nwritten;
    char c = keypressed;

    completionCallback(ls->buf,&lc);
    if (lc.len == 0) {
        linenoiseBeep();
        ls->in_completion = 0;
    } else {
        switch(c) {
            case 9: /* tab */
                if (ls->in_completion == 0) {
                    ls->in_completion = 1;
                    ls->completion_idx = 0;
                } else {
                    ls->completion_idx = (ls->completion_idx+1) % (lc.len+1);
                    if (ls->completion_idx == lc.len) linenoiseBeep();
                }
                c = 0;
                break;
            case 27: /* escape */
                /* Re-show original buffer */
                if (ls->completion_idx < lc.len) refreshLine(ls);
                ls->in_completion = 0;
                c = 0;
                break;
            default:
                /* Update buffer and return */
                if (ls->completion_idx < lc.len) {
                    nwritten = snprintf(ls->buf,ls->buflen,"%s",
                        lc.cvec[ls->completion_idx]);
                    ls->len = ls->pos = nwritten;
                }
                ls->in_completion = 0;
                break;
        }

        /* Show completion or original buffer */
        if (ls->in_completion && ls->completion_idx < lc.len) {
            refreshLineWithCompletion(ls,&lc,REFRESH_ALL);
        } else {
            refreshLine(ls);
        }
    }

    freeCompletions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn) {
    completionCallback = fn;
}

/* Register a hits function to be called to show hits to the user at the
 * right of the prompt. */
void linenoiseSetHintsCallback(linenoiseHintsCallback *fn) {
    hintsCallback = fn;
}

/* Register a function to free the hints returned by the hints callback
 * registered with linenoiseSetHintsCallback(). */
void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *fn) {
    freeHintsCallback = fn;
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str) {
#ifdef BP_EMBEDDED
    if (lc->len < BP_LINENOISE_COMPLETIONS_MAX) {
        lc->cvec[lc->len++] = str;
    }
#else
    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
#endif
}

/* =========================== Line editing ================================= */

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf {
#ifdef BP_EMBEDDED
    char b_static[BP_LINENOISE_MAX_LINE * 2]; /* Static buffer for embedded. */
    char *b;
    int len;
    int cap;
#else
    char *b;
    int len;
#endif
};

static void abInit(struct abuf *ab) {
#ifdef BP_EMBEDDED
    ab->b = ab->b_static;
    ab->len = 0;
    ab->cap = (int)sizeof(ab->b_static);
#else
    ab->b = NULL;
    ab->len = 0;
#endif
}

static void abAppend(struct abuf *ab, const char *s, int len) {
#ifdef BP_EMBEDDED
    if (ab->len + len > ab->cap) return; /* Truncate if too large. */
    memcpy(ab->b + ab->len, s, len);
    ab->len += len;
#else
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
#endif
}

static void abFree(struct abuf *ab) {
#ifdef BP_EMBEDDED
    /* Static buffer, nothing to free. */
    (void)ab;
#else
    free(ab->b);
#endif
}

/* Helper of refreshSingleLine() and refreshMultiLine() to show hints
 * to the right of the prompt. Now uses display widths for proper UTF-8. */
void refreshShowHints(struct abuf *ab, struct linenoiseState *l, int pwidth) {
    char seq[64];
    size_t bufwidth = utf8StrWidth(l->buf, l->len);
    if (hintsCallback && pwidth + bufwidth < l->cols) {
        int color = -1, bold = 0;
        char *hint = hintsCallback(l->buf,&color,&bold);
        if (hint) {
            size_t hintlen = strlen(hint);
            size_t hintwidth = utf8StrWidth(hint, hintlen);
            size_t hintmaxwidth = l->cols - (pwidth + bufwidth);
            /* Truncate hint to fit, respecting UTF-8 boundaries. */
            if (hintwidth > hintmaxwidth) {
                size_t i = 0, w = 0;
                while (i < hintlen) {
                    size_t clen = utf8NextCharLen(hint, i, hintlen);
                    int cwidth = utf8SingleCharWidth(hint + i, clen);
                    if (w + cwidth > hintmaxwidth) break;
                    w += cwidth;
                    i += clen;
                }
                hintlen = i;
            }
            if (bold == 1 && color == -1) color = 37;
            if (color != -1 || bold != 0)
                snprintf(seq,64,"\033[%d;%d;49m",bold,color);
            else
                seq[0] = '\0';
            abAppend(ab,seq,strlen(seq));
            abAppend(ab,hint,hintlen);
            if (color != -1 || bold != 0)
                abAppend(ab,"\033[0m",4);
            /* Call the function to free the hint returned. */
            if (freeHintsCallback) freeHintsCallback(hint);
        }
    }
}

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal.
 *
 * Flags is REFRESH_* macros. The function can just remove the old
 * prompt, just write it, or both.
 *
 * This function is UTF-8 aware and uses display widths (not byte counts)
 * for cursor positioning and horizontal scrolling. */
static void refreshSingleLine(struct linenoiseState *l, int flags) {
    char seq[64];
    size_t pwidth = utf8StrWidth(l->prompt, l->plen); /* Prompt display width */
    char *buf = l->buf;
    size_t len = l->len;    /* Byte length of buffer to display */
    size_t pos = l->pos;    /* Byte position of cursor */
    size_t poscol;          /* Display column of cursor */
    size_t lencol;          /* Display width of buffer */
    struct abuf ab;

    /* Calculate the display width up to cursor and total display width. */
    poscol = utf8StrWidth(buf, pos);
    lencol = utf8StrWidth(buf, len);

    /* Scroll the buffer horizontally if cursor is past the right edge.
     * We need to trim full UTF-8 characters from the left until the
     * cursor position fits within the terminal width. */
    while (pwidth + poscol >= l->cols) {
        size_t clen = utf8NextCharLen(buf, 0, len);
        int cwidth = utf8SingleCharWidth(buf, clen);
        buf += clen;
        len -= clen;
        pos -= clen;
        poscol -= cwidth;
        lencol -= cwidth;
    }

    /* Trim from the right if the line still doesn't fit. */
    while (pwidth + lencol > l->cols) {
        size_t clen = utf8PrevCharLen(buf, len);
        int cwidth = utf8SingleCharWidth(buf + len - clen, clen);
        len -= clen;
        lencol -= cwidth;
    }

    abInit(&ab);
    /* Cursor to left edge */
    snprintf(seq,sizeof(seq),"\r");
    abAppend(&ab,seq,strlen(seq));

    if (flags & REFRESH_WRITE) {
        /* Write the prompt and the current buffer content */
        abAppend(&ab,l->prompt,l->plen);
        if (maskmode == 1) {
            /* In mask mode, we output one '*' per UTF-8 character, not byte */
            size_t i = 0;
            while (i < len) {
                abAppend(&ab,"*",1);
                i += utf8NextCharLen(buf, i, len);
            }
        } else {
            abAppend(&ab,buf,len);
        }
        /* Show hints if any. */
        refreshShowHints(&ab,l,pwidth);
    }

    /* Erase to right */
    snprintf(seq,sizeof(seq),"\x1b[0K");
    abAppend(&ab,seq,strlen(seq));

    if (flags & REFRESH_WRITE) {
        /* Move cursor to original position (using display column, not byte). */
        snprintf(seq,sizeof(seq),"\r\x1b[%dC", (int)(poscol+pwidth));
        abAppend(&ab,seq,strlen(seq));
    }

    if (lnWrite(l,ab.b,ab.len) == -1) {} /* Can't recover from write error. */
    abFree(&ab);
}

/* Multi line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal.
 *
 * Flags is REFRESH_* macros. The function can just remove the old
 * prompt, just write it, or both.
 *
 * This function is UTF-8 aware and uses display widths for positioning. */
static void refreshMultiLine(struct linenoiseState *l, int flags) {
    char seq[64];
    size_t pwidth = utf8StrWidth(l->prompt, l->plen);  /* Prompt display width */
    size_t bufwidth = utf8StrWidth(l->buf, l->len);    /* Buffer display width */
    size_t poswidth = utf8StrWidth(l->buf, l->pos);    /* Cursor display width */
    int rows = (pwidth+bufwidth+l->cols-1)/l->cols;    /* rows used by current buf. */
    int rpos = l->oldrpos;   /* cursor relative row from previous refresh. */
    int rpos2; /* rpos after refresh. */
    int col; /* column position, zero-based. */
    int old_rows = l->oldrows;
    int j;
    struct abuf ab;

    l->oldrows = rows;

    /* First step: clear all the lines used before. To do so start by
     * going to the last row. */
    abInit(&ab);

    if (flags & REFRESH_CLEAN) {
        if (old_rows-rpos > 0) {
            lndebug("go down %d", old_rows-rpos);
            snprintf(seq,64,"\x1b[%dB", old_rows-rpos);
            abAppend(&ab,seq,strlen(seq));
        }

        /* Now for every row clear it, go up. */
        for (j = 0; j < old_rows-1; j++) {
            lndebug("clear+up");
            snprintf(seq,64,"\r\x1b[0K\x1b[1A");
            abAppend(&ab,seq,strlen(seq));
        }
    }

    if (flags & REFRESH_ALL) {
        /* Clean the top line. */
        lndebug("clear");
        snprintf(seq,64,"\r\x1b[0K");
        abAppend(&ab,seq,strlen(seq));
    }

    if (flags & REFRESH_WRITE) {
        /* Write the prompt and the current buffer content */
        abAppend(&ab,l->prompt,l->plen);
        if (maskmode == 1) {
            /* In mask mode, output one '*' per UTF-8 character, not byte */
            size_t i = 0;
            while (i < l->len) {
                abAppend(&ab,"*",1);
                i += utf8NextCharLen(l->buf, i, l->len);
            }
        } else {
            abAppend(&ab,l->buf,l->len);
        }

        /* Show hints if any. */
        refreshShowHints(&ab,l,pwidth);

        /* If we are at the very end of the screen with our prompt, we need to
         * emit a newline and move the prompt to the first column. */
        if (l->pos &&
            l->pos == l->len &&
            (poswidth+pwidth) % l->cols == 0)
        {
            lndebug("<newline>");
            abAppend(&ab,"\n",1);
            snprintf(seq,64,"\r");
            abAppend(&ab,seq,strlen(seq));
            rows++;
            if (rows > (int)l->oldrows) l->oldrows = rows;
        }

        /* Move cursor to right position. */
        rpos2 = (pwidth+poswidth+l->cols)/l->cols; /* Current cursor relative row */
        lndebug("rpos2 %d", rpos2);

        /* Go up till we reach the expected position. */
        if (rows-rpos2 > 0) {
            lndebug("go-up %d", rows-rpos2);
            snprintf(seq,64,"\x1b[%dA", rows-rpos2);
            abAppend(&ab,seq,strlen(seq));
        }

        /* Set column. */
        col = (pwidth+poswidth) % l->cols;
        lndebug("set col %d", 1+col);
        if (col)
            snprintf(seq,64,"\r\x1b[%dC", col);
        else
            snprintf(seq,64,"\r");
        abAppend(&ab,seq,strlen(seq));
    }

    lndebug("\n");
    l->oldpos = l->pos;
    if (flags & REFRESH_WRITE) l->oldrpos = rpos2;

    if (lnWrite(l,ab.b,ab.len) == -1) {} /* Can't recover from write error. */
    abFree(&ab);
}

/* Calls the two low level functions refreshSingleLine() or
 * refreshMultiLine() according to the selected mode. */
static void refreshLineWithFlags(struct linenoiseState *l, int flags) {
    if (mlmode)
        refreshMultiLine(l,flags);
    else
        refreshSingleLine(l,flags);
}

/* Utility function to avoid specifying REFRESH_ALL all the times. */
static void refreshLine(struct linenoiseState *l) {
    refreshLineWithFlags(l,REFRESH_ALL);
}

/* Hide the current line, when using the multiplexing API. */
void linenoiseHide(struct linenoiseState *l) {
    if (mlmode)
        refreshMultiLine(l,REFRESH_CLEAN);
    else
        refreshSingleLine(l,REFRESH_CLEAN);
}

/* Show the current line, when using the multiplexing API. */
void linenoiseShow(struct linenoiseState *l) {
    if (l->in_completion) {
        refreshLineWithCompletion(l,NULL,REFRESH_WRITE);
    } else {
        refreshLineWithFlags(l,REFRESH_WRITE);
    }
}

/* Insert the character(s) 'c' of length 'clen' at cursor current position.
 * This handles both single-byte ASCII and multi-byte UTF-8 sequences.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
int linenoiseEditInsert(struct linenoiseState *l, const char *c, size_t clen) {
    if (l->len + clen <= l->buflen) {
        if (l->len == l->pos) {
            /* Append at end of line. */
            memcpy(l->buf+l->pos, c, clen);
            l->pos += clen;
            l->len += clen;
            l->buf[l->len] = '\0';
            if ((!mlmode &&
                 utf8StrWidth(l->prompt,l->plen)+utf8StrWidth(l->buf,l->len) < l->cols &&
                 !hintsCallback)) {
                /* Avoid a full update of the line in the trivial case:
                 * single-width char, no hints, fits in one line. */
                if (maskmode == 1) {
                    if (lnWrite(l,"*",1) == -1) return -1;
                } else {
                    if (lnWrite(l,c,clen) == -1) return -1;
                }
            } else {
                refreshLine(l);
            }
        } else {
            /* Insert in the middle of the line. */
            memmove(l->buf+l->pos+clen, l->buf+l->pos, l->len-l->pos);
            memcpy(l->buf+l->pos, c, clen);
            l->len += clen;
            l->pos += clen;
            l->buf[l->len] = '\0';
            refreshLine(l);
        }
    }
    return 0;
}

/* Move cursor on the left. Moves by one UTF-8 character, not byte. */
void linenoiseEditMoveLeft(struct linenoiseState *l) {
    if (l->pos > 0) {
        l->pos -= utf8PrevCharLen(l->buf, l->pos);
        refreshLine(l);
    }
}

/* Move cursor on the right. Moves by one UTF-8 character, not byte. */
void linenoiseEditMoveRight(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos += utf8NextCharLen(l->buf, l->pos, l->len);
        refreshLine(l);
    }
}

/* Move cursor to the start of the line. */
void linenoiseEditMoveHome(struct linenoiseState *l) {
    if (l->pos != 0) {
        l->pos = 0;
        refreshLine(l);
    }
}

/* Move cursor to the end of the line. */
void linenoiseEditMoveEnd(struct linenoiseState *l) {
    if (l->pos != l->len) {
        l->pos = l->len;
        refreshLine(l);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'.
 * NOTE: This is now defined later in the file, after the #ifndef BP_EMBEDDED
 * block, with proper gating for static vs dynamic allocation. */
#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
/* Forward declaration - defined at end of file. */
void linenoiseEditHistoryNext(struct linenoiseState *l, int dir);

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key.
 * Now handles multi-byte UTF-8 characters. */
void linenoiseEditDelete(struct linenoiseState *l) {
    if (l->len > 0 && l->pos < l->len) {
        size_t clen = utf8NextCharLen(l->buf, l->pos, l->len);
        memmove(l->buf+l->pos, l->buf+l->pos+clen, l->len-l->pos-clen);
        l->len -= clen;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Backspace implementation. Deletes the UTF-8 character before the cursor. */
void linenoiseEditBackspace(struct linenoiseState *l) {
    if (l->pos > 0 && l->len > 0) {
        size_t clen = utf8PrevCharLen(l->buf, l->pos);
        memmove(l->buf+l->pos-clen, l->buf+l->pos, l->len-l->pos);
        l->pos -= clen;
        l->len -= clen;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Delete the previous word, maintaining the cursor at the start of the
 * current word. Handles UTF-8 by moving character-by-character. */
void linenoiseEditDeletePrevWord(struct linenoiseState *l) {
    size_t old_pos = l->pos;
    size_t diff;

    /* Skip spaces before the word (move backwards by UTF-8 chars). */
    while (l->pos > 0 && l->buf[l->pos-1] == ' ')
        l->pos -= utf8PrevCharLen(l->buf, l->pos);
    /* Skip non-space characters (move backwards by UTF-8 chars). */
    while (l->pos > 0 && l->buf[l->pos-1] != ' ')
        l->pos -= utf8PrevCharLen(l->buf, l->pos);
    diff = old_pos - l->pos;
    memmove(l->buf+l->pos, l->buf+old_pos, l->len-old_pos+1);
    l->len -= diff;
    refreshLine(l);
}

/* This function is part of the multiplexed API of Linenoise, that is used
 * in order to implement the blocking variant of the API but can also be
 * called by the user directly in an event driven program. It will:
 *
 * 1. Initialize the linenoise state passed by the user.
 * 2. Put the terminal in RAW mode.
 * 3. Show the prompt.
 * 4. Return control to the user, that will have to call linenoiseEditFeed()
 *    each time there is some data arriving in the standard input.
 *
 * The user can also call linenoiseEditHide() and linenoiseEditShow() if it
 * is required to show some input arriving asyncronously, without mixing
 * it with the currently edited line.
 *
 * When linenoiseEditFeed() returns non-NULL, the user finished with the
 * line editing session (pressed enter CTRL-D/C): in this case the caller
 * needs to call linenoiseEditStop() to put back the terminal in normal
 * mode. This will not destroy the buffer, as long as the linenoiseState
 * is still valid in the context of the caller.
 *
 * The function returns 0 on success, or -1 if writing to standard output
 * fails. If stdin_fd or stdout_fd are set to -1, the default is to use
 * STDIN_FILENO and STDOUT_FILENO.
 */
int linenoiseEditStart(struct linenoiseState *l, int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt) {
    /* Populate the linenoise state that we pass to functions implementing
     * specific editing functionalities. */
    l->in_completion = 0;
#ifdef BP_EMBEDDED
    (void)stdin_fd; (void)stdout_fd; (void)buf; (void)buflen; /* Unused in embedded mode. */
    l->buflen = BP_LINENOISE_MAX_LINE;
#else
    l->ifd = stdin_fd != -1 ? stdin_fd : STDIN_FILENO;
    l->ofd = stdout_fd != -1 ? stdout_fd : STDOUT_FILENO;
    l->buf = buf;
    l->buflen = buflen;
#endif
    l->prompt = prompt;
    l->plen = strlen(prompt);
    l->oldpos = l->pos = 0;
    l->len = 0;

#ifndef BP_EMBEDDED
    /* Enter raw mode. */
    if (enableRawMode(l->ifd) == -1) return -1;

    l->cols = getColumns(stdin_fd, stdout_fd);
#endif
    l->oldrows = 0;
    l->oldrpos = 1;  /* Cursor starts on row 1. */
    l->history_index = 0;

    /* Buffer starts empty. */
    l->buf[0] = '\0';
    l->buflen--; /* Make sure there is always space for the nulterm */

#ifndef BP_EMBEDDED
    /* If stdin is not a tty, stop here with the initialization. We
     * will actually just read a line from standard input in blocking
     * mode later, in linenoiseEditFeed(). */
    if (!isatty(l->ifd) && !getenv("LINENOISE_ASSUME_TTY")) return 0;
#endif

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");

    if (lnWrite(l,prompt,l->plen) == -1) return -1;
    return 0;
}

char *linenoiseEditMore = "If you see this, you are misusing the API: when linenoiseEditFeed() is called, if it returns linenoiseEditMore the user is yet editing the line. See the README file for more information.";

/* This function is part of the multiplexed API of linenoise, see the top
 * comment on linenoiseEditStart() for more information. Call this function
 * each time there is some data to read from the standard input file
 * descriptor. In the case of blocking operations, this function can just be
 * called in a loop, and block.
 *
 * The function returns linenoiseEditMore to signal that line editing is still
 * in progress, that is, the user didn't yet pressed enter / CTRL-D. Otherwise
 * the function returns the pointer to the heap-allocated buffer with the
 * edited line, that the user should free with linenoiseFree().
 *
 * On special conditions, NULL is returned and errno is populated:
 *
 * EAGAIN if the user pressed Ctrl-C
 * ENOENT if the user pressed Ctrl-D
 *
 * Some other errno: I/O error.
 */
char *linenoiseEditFeed(struct linenoiseState *l) {
#ifndef BP_EMBEDDED
    /* Not a TTY, pass control to line reading without character
     * count limits. */
    if (!isatty(l->ifd) && !getenv("LINENOISE_ASSUME_TTY")) return linenoiseNoTTY();
#endif

    char c;
    int nread;
    char seq[3];

    nread = lnRead(l,&c,1);
    if (nread < 0) {
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? linenoiseEditMore : NULL;
    } else if (nread == 0) {
#ifdef BP_EMBEDDED
        return linenoiseEditMore; /* No data available (non-blocking). */
#else
        return NULL;
#endif
    }

    /* Only autocomplete when the callback is set. It returns < 0 when
     * there was an error reading from fd. Otherwise it will return the
     * character that should be handled next. */
    if ((l->in_completion || c == 9) && completionCallback != NULL) {
#ifdef BP_EMBEDDED
        if (!l->simple_mode) {
#endif
        c = completeLine(l,c);
        /* Return on errors */
        if (c < 0) return NULL;
        /* Read next character when 0 */
        if (c == 0) return linenoiseEditMore;
#ifdef BP_EMBEDDED
        }
#endif
    }

    switch(c) {
    case ENTER:    /* enter */
        history_len--;
#ifndef BP_EMBEDDED
        free(history[history_len]);
#endif
        if (mlmode) linenoiseEditMoveEnd(l);
        if (hintsCallback) {
            /* Force a refresh without hints to leave the previous
             * line as the user typed it after a newline. */
            linenoiseHintsCallback *hc = hintsCallback;
            hintsCallback = NULL;
            refreshLine(l);
            hintsCallback = hc;
        }
#ifdef BP_EMBEDDED
        return l->buf; /* Return pointer to static buffer. */
#else
        return strdup(l->buf);
#endif
    case CTRL_C:     /* ctrl-c */
        errno = EAGAIN;
        return NULL;
    case BACKSPACE:   /* backspace */
    case 8:     /* ctrl-h */
#ifdef BP_EMBEDDED
        if (l->simple_mode) {
            /* Simple mode: use basic terminal backspace. */
            if (l->pos > 0 && l->len > 0) {
                memmove(l->buf + l->pos - 1,
                        l->buf + l->pos,
                        l->len - l->pos);
                l->pos--;
                l->len--;
                l->buf[l->len] = '\0';
                lnWrite(l, "\b \b", 3);
            }
        } else
#endif
        linenoiseEditBackspace(l);
        break;
    case CTRL_D:     /* ctrl-d, remove char at right of cursor, or if the
                        line is empty, act as end-of-file. */
        if (l->len > 0) {
#ifdef BP_EMBEDDED
            if (!l->simple_mode)
#endif
            linenoiseEditDelete(l);
        } else {
            history_len--;
#ifndef BP_EMBEDDED
            free(history[history_len]);
#endif
            errno = ENOENT;
            return NULL;
        }
        break;
    case CTRL_T:    /* ctrl-t, swaps current character with previous. */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        /* Handle UTF-8: swap the two UTF-8 characters around cursor. */
        if (l->pos > 0 && l->pos < l->len) {
            char tmp[32];
            size_t prevlen = utf8PrevCharLen(l->buf, l->pos);
            size_t currlen = utf8NextCharLen(l->buf, l->pos, l->len);
            size_t prevstart = l->pos - prevlen;
            /* Copy current char to tmp, move previous char right, paste tmp. */
            memcpy(tmp, l->buf + l->pos, currlen);
            memmove(l->buf + prevstart + currlen, l->buf + prevstart, prevlen);
            memcpy(l->buf + prevstart, tmp, currlen);
            if (l->pos + currlen <= l->len) l->pos += currlen;
            refreshLine(l);
        }
        break;
    case CTRL_B:     /* ctrl-b */
#ifdef BP_EMBEDDED
        /* In Bus Pirate, Ctrl+B is used for screen refresh. */
        errno = 0;
        return NULL; /* Caller checks errno: 0 means refresh. */
#else
        linenoiseEditMoveLeft(l);
#endif
        break;
    case CTRL_F:     /* ctrl-f */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        linenoiseEditMoveRight(l);
        break;
    case CTRL_P:    /* ctrl-p */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);
        break;
    case CTRL_N:    /* ctrl-n */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);
        break;
    case ESC:    /* escape sequence */
        /* Read the next two bytes representing the escape sequence.
         * Use two calls to handle slow terminals returning the two
         * chars at different times. */
        if (lnReadBlocking(l,seq,1) == -1) break;
        if (lnReadBlocking(l,seq+1,1) == -1) break;

        /* ESC [ sequences. */
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                /* Extended escape, read additional byte. */
                if (lnReadBlocking(l,seq+2,1) == -1) break;
                if (seq[2] == '~') {
                    switch(seq[1]) {
                    case '1': /* Home (some terminals). */
#ifdef BP_EMBEDDED
                        if (!l->simple_mode)
#endif
                        linenoiseEditMoveHome(l);
                        break;
                    case '3': /* Delete key. */
#ifdef BP_EMBEDDED
                        if (!l->simple_mode)
#endif
                        linenoiseEditDelete(l);
                        break;
                    case '4': /* End (some terminals). */
#ifdef BP_EMBEDDED
                        if (!l->simple_mode)
#endif
                        linenoiseEditMoveEnd(l);
                        break;
                    }
                }
            } else {
                switch(seq[1]) {
                case 'A': /* Up */
#ifdef BP_EMBEDDED
                    if (!l->simple_mode)
#endif
                    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);
                    break;
                case 'B': /* Down */
#ifdef BP_EMBEDDED
                    if (!l->simple_mode)
#endif
                    linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);
                    break;
                case 'C': /* Right */
#ifdef BP_EMBEDDED
                    if (!l->simple_mode)
#endif
                    linenoiseEditMoveRight(l);
                    break;
                case 'D': /* Left */
#ifdef BP_EMBEDDED
                    if (!l->simple_mode)
#endif
                    linenoiseEditMoveLeft(l);
                    break;
                case 'H': /* Home */
#ifdef BP_EMBEDDED
                    if (!l->simple_mode)
#endif
                    linenoiseEditMoveHome(l);
                    break;
                case 'F': /* End*/
#ifdef BP_EMBEDDED
                    if (!l->simple_mode)
#endif
                    linenoiseEditMoveEnd(l);
                    break;
                }
            }
        }

        /* ESC O sequences. */
        else if (seq[0] == 'O') {
            switch(seq[1]) {
            case 'H': /* Home */
#ifdef BP_EMBEDDED
                if (!l->simple_mode)
#endif
                linenoiseEditMoveHome(l);
                break;
            case 'F': /* End*/
#ifdef BP_EMBEDDED
                if (!l->simple_mode)
#endif
                linenoiseEditMoveEnd(l);
                break;
            }
        }
        break;
    default:
        /* Handle UTF-8 multi-byte sequences. When we receive the first byte
         * of a multi-byte UTF-8 character, read the remaining bytes to
         * complete the sequence before inserting. */
        {
            char utf8[4];
            int utf8len = utf8ByteLen(c);
            utf8[0] = c;
            if (utf8len > 1) {
                /* Read remaining bytes of the UTF-8 sequence. */
                int i;
                for (i = 1; i < utf8len; i++) {
                    if (lnReadBlocking(l, utf8+i, 1) != 1) break;
                }
            }
            if (linenoiseEditInsert(l, utf8, utf8len)) return NULL;
        }
        break;
    case CTRL_U: /* Ctrl+u, delete the whole line. */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        {
            l->buf[0] = '\0';
            l->pos = l->len = 0;
            refreshLine(l);
        }
        break;
    case CTRL_K: /* Ctrl+k, delete from current to end of line. */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        {
            l->buf[l->pos] = '\0';
            l->len = l->pos;
            refreshLine(l);
        }
        break;
    case CTRL_A: /* Ctrl+a, go to the start of the line */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        linenoiseEditMoveHome(l);
        break;
    case CTRL_E: /* ctrl+e, go to the end of the line */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        linenoiseEditMoveEnd(l);
        break;
    case CTRL_L: /* ctrl+l, clear screen */
#ifdef BP_EMBEDDED
        if (!l->simple_mode) {
            lnWrite(l, "\x1b[H\x1b[2J", 7);
            refreshLine(l);
        }
#else
        linenoiseClearScreen();
        refreshLine(l);
#endif
        break;
    case CTRL_W: /* ctrl+w, delete previous word */
#ifdef BP_EMBEDDED
        if (!l->simple_mode)
#endif
        linenoiseEditDeletePrevWord(l);
        break;
    }
    return linenoiseEditMore;
}

/* This is part of the multiplexed linenoise API. See linenoiseEditStart()
 * for more information. This function is called when linenoiseEditFeed()
 * returns something different than NULL. At this point the user input
 * is in the buffer, and we can restore the terminal in normal mode. */
void linenoiseEditStop(struct linenoiseState *l) {
#ifdef BP_EMBEDDED
    lnWrite(l, "\n", 1);
    l->buf[l->len] = '\0';
#else
    if (!isatty(l->ifd) && !getenv("LINENOISE_ASSUME_TTY")) return;
    disableRawMode(l->ifd);
    printf("\n");
#endif
}

#ifndef BP_EMBEDDED
/* This just implements a blocking loop for the multiplexed API.
 * In many applications that are not event-drivern, we can just call
 * the blocking linenoise API, wait for the user to complete the editing
 * and return the buffer. */
static char *linenoiseBlockingEdit(int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt)
{
    struct linenoiseState l;

    /* Editing without a buffer is invalid. */
    if (buflen == 0) {
        errno = EINVAL;
        return NULL;
    }

    linenoiseEditStart(&l,stdin_fd,stdout_fd,buf,buflen,prompt);
    char *res;
    while((res = linenoiseEditFeed(&l)) == linenoiseEditMore);
    linenoiseEditStop(&l);
    return res;
}

/* This special mode is used by linenoise in order to print scan codes
 * on screen for debugging / development purposes. It is implemented
 * by the linenoise_example program using the --keycodes option. */
void linenoisePrintKeyCodes(void) {
    char quit[4];

    printf("Linenoise key codes debugging mode.\n"
            "Press keys to see scan codes. Type 'quit' at any time to exit.\n");
    if (enableRawMode(STDIN_FILENO) == -1) return;
    memset(quit,' ',4);
    while(1) {
        char c;
        int nread;

        nread = read(STDIN_FILENO,&c,1);
        if (nread <= 0) continue;
        memmove(quit,quit+1,sizeof(quit)-1); /* shift string to left. */
        quit[sizeof(quit)-1] = c; /* Insert current char on the right. */
        if (memcmp(quit,"quit",sizeof(quit)) == 0) break;

        printf("'%c' %02x (%d) (type quit to exit)\n",
            isprint(c) ? c : '?', (int)c, (int)c);
        printf("\r"); /* Go left edge manually, we are in raw mode. */
        fflush(stdout);
    }
    disableRawMode(STDIN_FILENO);
}

/* This function is called when linenoise() is called with the standard
 * input file descriptor not attached to a TTY. So for example when the
 * program using linenoise is called in pipe or with a file redirected
 * to its standard input. In this case, we want to be able to return the
 * line regardless of its length (by default we are limited to 4k). */
static char *linenoiseNoTTY(void) {
    char *line = NULL;
    size_t len = 0, maxlen = 0;

    while(1) {
        if (len == maxlen) {
            if (maxlen == 0) maxlen = 16;
            maxlen *= 2;
            char *oldval = line;
            line = realloc(line,maxlen);
            if (line == NULL) {
                if (oldval) free(oldval);
                return NULL;
            }
        }
        int c = fgetc(stdin);
        if (c == EOF || c == '\n') {
            if (c == EOF && len == 0) {
                free(line);
                return NULL;
            } else {
                line[len] = '\0';
                return line;
            }
        } else {
            line[len] = c;
            len++;
        }
    }
}

/* The high level function that is the main API of the linenoise library.
 * This function checks if the terminal has basic capabilities, just checking
 * for a blacklist of stupid terminals, and later either calls the line
 * editing function or uses dummy fgets() so that you will be able to type
 * something even in the most desperate of the conditions. */
char *linenoise(const char *prompt) {
    char buf[LINENOISE_MAX_LINE];

    if (!isatty(STDIN_FILENO) && !getenv("LINENOISE_ASSUME_TTY")) {
        /* Not a tty: read from file / pipe. In this mode we don't want any
         * limit to the line size, so we call a function to handle that. */
        return linenoiseNoTTY();
    } else if (isUnsupportedTerm()) {
        size_t len;

        printf("%s",prompt);
        fflush(stdout);
        if (fgets(buf,LINENOISE_MAX_LINE,stdin) == NULL) return NULL;
        len = strlen(buf);
        while(len && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            len--;
            buf[len] = '\0';
        }
        return strdup(buf);
    } else {
        char *retval = linenoiseBlockingEdit(STDIN_FILENO,STDOUT_FILENO,buf,LINENOISE_MAX_LINE,prompt);
        return retval;
    }
}

/* This is just a wrapper the user may want to call in order to make sure
 * the linenoise returned buffer is freed with the same allocator it was
 * created with. Useful when the main program is using an alternative
 * allocator. */
void linenoiseFree(void *ptr) {
    if (ptr == linenoiseEditMore) return; // Protect from API misuse.
    free(ptr);
}
#endif /* !BP_EMBEDDED */

/* ================================ History ================================= */

/* Free the history, but does not reset it. Only used when we have to
 * exit() to avoid memory leaks are reported by valgrind & co. */
static void freeHistory(void) {
#ifdef BP_EMBEDDED
    history_len = 0;
#else
    if (history) {
        int j;

        for (j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
    }
#endif
}

/* At exit we'll try to fix the terminal to the initial conditions. */
#ifndef BP_EMBEDDED
static void linenoiseAtExit(void) {
    disableRawMode(STDIN_FILENO);
    freeHistory();
}
#endif

/* This is the API call to add a new entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the new one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const char *line) {
#ifdef BP_EMBEDDED
    if (history_max_len == 0) return 0;

    /* One-time init: point history_ptrs at static storage. */
    if (!history_inited) {
        int j;
        for (j = 0; j < BP_LINENOISE_HISTORY_MAX; j++)
            history_ptrs[j] = history_storage[j];
        history_inited = 1;
    }

    /* Don't add duplicated lines. */
    if (history_len && !strcmp(history[history_len-1], line)) return 0;

    /* If we reached the max length, shift entries down (discard oldest). */
    if (history_len == history_max_len) {
        /* Rotate pointers: save ptr to slot 0, shift rest down, reuse. */
        char *oldest = history[0];
        memmove(history, history+1, sizeof(char*)*(history_max_len-1));
        history[history_max_len-1] = oldest;
        history_len--;
    }
    strncpy(history[history_len], line, BP_LINENOISE_MAX_LINE);
    history[history_len][BP_LINENOISE_MAX_LINE] = '\0';
    history_len++;
    return 1;
#else
    char *linecopy;

    if (history_max_len == 0) return 0;

    /* Initialization on first call. */
    if (history == NULL) {
        history = malloc(sizeof(char*)*history_max_len);
        if (history == NULL) return 0;
        memset(history,0,(sizeof(char*)*history_max_len));
    }

    /* Don't add duplicated lines. */
    if (history_len && !strcmp(history[history_len-1], line)) return 0;

    /* Add an heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line. */
    linecopy = strdup(line);
    if (!linecopy) return 0;
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history,history+1,sizeof(char*)*(history_max_len-1));
        history_len--;
    }
    history[history_len] = linecopy;
    history_len++;
    return 1;
#endif
}

#ifndef BP_EMBEDDED
/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the new history length value is smaller
 * than the amount of items already inside the history. */
int linenoiseHistorySetMaxLen(int len) {
    char **new;

    if (len < 1) return 0;
    if (history) {
        int tocopy = history_len;

        new = malloc(sizeof(char*)*len);
        if (new == NULL) return 0;

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy) {
            int j;

            for (j = 0; j < tocopy-len; j++) free(history[j]);
            tocopy = len;
        }
        memset(new,0,sizeof(char*)*len);
        memcpy(new,history+(history_len-tocopy), sizeof(char*)*tocopy);
        free(history);
        history = new;
    }
    history_max_len = len;
    if (history_len > history_max_len)
        history_len = history_max_len;
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const char *filename) {
    mode_t old_umask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
    FILE *fp;
    int j;

    fp = fopen(filename,"w");
    umask(old_umask);
    if (fp == NULL) return -1;
    fchmod(fileno(fp),S_IRUSR|S_IWUSR);
    for (j = 0; j < history_len; j++)
        fprintf(fp,"%s\n",history[j]);
    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(const char *filename) {
    FILE *fp = fopen(filename,"r");
    char buf[LINENOISE_MAX_LINE];

    if (fp == NULL) return -1;

    while (fgets(buf,LINENOISE_MAX_LINE,fp) != NULL) {
        char *p;

        p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(buf);
    }
    fclose(fp);
    return 0;
}
#endif /* !BP_EMBEDDED */

/* ======================== Shared utility functions ======================== */

/* Clear history. */
void linenoiseHistoryClear(void) {
    freeHistory();
    history_len = 0;
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
void linenoiseEditHistoryNext(struct linenoiseState *l, int dir) {
    if (history_len > 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
#ifdef BP_EMBEDDED
        strncpy(history[history_len - 1 - l->history_index], l->buf, BP_LINENOISE_MAX_LINE);
        history[history_len - 1 - l->history_index][BP_LINENOISE_MAX_LINE] = '\0';
#else
        free(history[history_len - 1 - l->history_index]);
        history[history_len - 1 - l->history_index] = strdup(l->buf);
#endif
        /* Show the new entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= history_len) {
            l->history_index = history_len-1;
            return;
        }
        strncpy(l->buf,history[history_len - 1 - l->history_index],l->buflen);
        l->buf[l->buflen-1] = '\0';
        l->len = l->pos = strlen(l->buf);
        refreshLine(l);
    }
}

/* ======================== BP_EMBEDDED API ================================= */

#ifdef BP_EMBEDDED

void linenoiseSetCallbacks(struct linenoiseState *l,
                           linenoiseReadCharFn try_read,
                           linenoiseReadBlockingFn read_blocking,
                           linenoiseWriteFn write_fn,
                           size_t cols) {
    memset(l, 0, sizeof(*l));
    l->try_read = try_read;
    l->read_blocking = read_blocking;
    l->write_fn = write_fn;
    l->cols = cols;
    l->buflen = BP_LINENOISE_MAX_LINE;
}

void linenoiseStartEdit(struct linenoiseState *l, const char *prompt) {
    l->prompt = prompt;
    l->plen = strlen(prompt);
    l->pos = 0;
    l->len = 0;
    l->buf[0] = '\0';
    l->history_index = 0;
    l->in_completion = 0;
    l->oldrows = 0;
    l->oldrpos = 1;

    /* The latest history entry is always our current buffer. */
    linenoiseHistoryAdd("");

    lnWrite(l, prompt, l->plen);
}

linenoiseResult linenoiseEditFeedResult(struct linenoiseState *l) {
    char *res = linenoiseEditFeed(l);
    if (res == linenoiseEditMore) return LN_CONTINUE;
    if (res == NULL) {
        if (errno == EAGAIN) return LN_CTRL_C;
        if (errno == 0) return LN_REFRESH;
        return LN_CTRL_D;
    }
    /* res is l->buf (static), line is complete. */
    return LN_ENTER;
}

void linenoiseSetSimpleMode(struct linenoiseState *l, int enable) {
    l->simple_mode = enable;
}

void linenoiseSetCols(struct linenoiseState *l, size_t cols) {
    l->cols = cols;
}

#endif /* BP_EMBEDDED */
