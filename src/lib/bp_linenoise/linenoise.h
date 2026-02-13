/* linenoise.h -- VERSION 1.0
 *
 * Guerrilla line editing library against the idea that a line editing lib
 * needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
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
 */

#ifndef __LINENOISE_H
#define __LINENOISE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* For size_t. */

#ifdef BP_EMBEDDED
#include <stdbool.h>
#include <stdint.h>
#endif

extern char *linenoiseEditMore;

/* ======================== BP_EMBEDDED configuration ======================= */

#ifdef BP_EMBEDDED

/* Tunable limits for embedded targets. Override before including this header. */
#ifndef BP_LINENOISE_MAX_LINE
#define BP_LINENOISE_MAX_LINE 256
#endif

#ifndef BP_LINENOISE_HISTORY_MAX
#define BP_LINENOISE_HISTORY_MAX 8
#endif

#ifndef BP_LINENOISE_COMPLETIONS_MAX
#define BP_LINENOISE_COMPLETIONS_MAX 16
#endif

/* Override the upstream LINENOISE_MAX_LINE with the embedded value. */
#undef LINENOISE_MAX_LINE
#define LINENOISE_MAX_LINE BP_LINENOISE_MAX_LINE

/* I/O callback types for embedded (no POSIX file descriptors). */
typedef bool (*linenoiseReadCharFn)(char *c);           /* Non-blocking read */
typedef void (*linenoiseReadBlockingFn)(char *c);       /* Blocking read (for ESC seqs) */
typedef void (*linenoiseWriteFn)(const char *s, size_t len); /* Write bytes */

/* Result codes for linenoiseEditFeedResult(). */
typedef enum {
    LN_CONTINUE = 0,    /* Still editing, need more input */
    LN_ENTER,           /* User pressed Enter, line complete */
    LN_CTRL_C,          /* User pressed Ctrl+C */
    LN_CTRL_D,          /* User pressed Ctrl+D (EOF on empty line) */
    LN_REFRESH,         /* Screen refresh requested (Ctrl+B on Bus Pirate) */
} linenoiseResult;

#endif /* BP_EMBEDDED */

/* ========================= linenoiseState ================================ */

/* The linenoiseState structure represents the state during line editing.
 * We pass this state to functions implementing specific editing
 * functionalities. */
struct linenoiseState {
    int in_completion;  /* The user pressed TAB and we are now in completion
                         * mode, so input is handled by completeLine(). */
    size_t completion_idx; /* Index of next completion to propose. */
#ifdef BP_EMBEDDED
    char buf[BP_LINENOISE_MAX_LINE + 1]; /* Static edit buffer. */
    linenoiseReadCharFn try_read;        /* Non-blocking char read callback. */
    linenoiseReadBlockingFn read_blocking; /* Blocking char read callback. */
    linenoiseWriteFn write_fn;           /* Write callback. */
    int simple_mode;    /* Simple mode: no history, no full-line refresh. */
#else
    int ifd;            /* Terminal stdin file descriptor. */
    int ofd;            /* Terminal stdout file descriptor. */
    char *buf;          /* Edited line buffer. */
#endif
    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    size_t oldrows;     /* Rows used by last refreshed line (multiline mode) */
    int oldrpos;        /* Cursor row from last refresh (for multiline clearing). */
    int history_index;  /* The history index we are currently editing. */
};

/* ========================= Completions =================================== */

typedef struct linenoiseCompletions {
#ifdef BP_EMBEDDED
    size_t len;
    const char *cvec[BP_LINENOISE_COMPLETIONS_MAX]; /* Static array of pointers (no alloc). */
#else
    size_t len;
    char **cvec;
#endif
} linenoiseCompletions;

/* Non blocking API. */
int linenoiseEditStart(struct linenoiseState *l, int stdin_fd, int stdout_fd, char *buf, size_t buflen, const char *prompt);
char *linenoiseEditFeed(struct linenoiseState *l);
void linenoiseEditStop(struct linenoiseState *l);
void linenoiseHide(struct linenoiseState *l);
void linenoiseShow(struct linenoiseState *l);

#ifdef BP_EMBEDDED
/* Embedded-specific init/start that use callbacks instead of fds. */
void linenoiseSetCallbacks(struct linenoiseState *l,
                           linenoiseReadCharFn try_read,
                           linenoiseReadBlockingFn read_blocking,
                           linenoiseWriteFn write_fn,
                           size_t cols);
void linenoiseStartEdit(struct linenoiseState *l, const char *prompt);
linenoiseResult linenoiseEditFeedResult(struct linenoiseState *l);
void linenoiseSetSimpleMode(struct linenoiseState *l, int enable);
void linenoiseSetCols(struct linenoiseState *l, size_t cols);
#endif

/* Blocking API. */
char *linenoise(const char *prompt);
void linenoiseFree(void *ptr);

/* Completion API. */
typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
typedef char*(linenoiseHintsCallback)(const char *, int *color, int *bold);
typedef void(linenoiseFreeHintsCallback)(void *);
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *);
void linenoiseSetHintsCallback(linenoiseHintsCallback *);
void linenoiseSetFreeHintsCallback(linenoiseFreeHintsCallback *);
void linenoiseAddCompletion(linenoiseCompletions *, const char *);

/* History API. */
int linenoiseHistoryAdd(const char *line);
int linenoiseHistorySetMaxLen(int len);
#ifndef BP_EMBEDDED
int linenoiseHistorySave(const char *filename);
int linenoiseHistoryLoad(const char *filename);
#endif
void linenoiseHistoryClear(void);

/* Other utilities. */
void linenoiseClearScreen(void);
void linenoiseSetMultiLine(int ml);
void linenoisePrintKeyCodes(void);
void linenoiseMaskModeEnable(void);
void linenoiseMaskModeDisable(void);

/* Editing functions (public for programmatic control). */
int linenoiseEditInsert(struct linenoiseState *l, const char *c, size_t clen);
void linenoiseEditMoveLeft(struct linenoiseState *l);
void linenoiseEditMoveRight(struct linenoiseState *l);
void linenoiseEditMoveHome(struct linenoiseState *l);
void linenoiseEditMoveEnd(struct linenoiseState *l);
void linenoiseEditDelete(struct linenoiseState *l);
void linenoiseEditBackspace(struct linenoiseState *l);
void linenoiseEditDeletePrevWord(struct linenoiseState *l);
void linenoiseEditHistoryNext(struct linenoiseState *l, int dir);

#ifdef __cplusplus
}
#endif

#endif /* __LINENOISE_H */
