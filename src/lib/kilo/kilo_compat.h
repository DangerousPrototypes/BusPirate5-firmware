/*
 * kilo_compat.h — POSIX compatibility shim for kilo editor on Bus Pirate 5/6
 *
 * This header replaces the POSIX headers that kilo.c normally includes.
 * It provides stub types, constants, and #define macros that redirect
 * POSIX I/O calls to Bus Pirate platform functions (USB FIFO, FatFS, etc).
 *
 * kilo.c remains as close to upstream (antirez/kilo) as possible — only
 * the #include block and main() need #ifdef BUSPIRATE patches.
 * Everything else is handled here via preprocessor redirection.
 */

#ifndef KILO_COMPAT_H
#define KILO_COMPAT_H

/* ---- Standard C library (available in Pico SDK newlib) ---- */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <stdarg.h>
#include <setjmp.h>

/* ---- Pico SDK ---- */
#include "pico/stdlib.h"

/* ---- Bus Pirate platform ---- */
#include "pirate.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"

/* ======================================================================
 * POSIX type/constant stubs — for headers that don't exist on Pico SDK
 * ====================================================================== */

/* termios.h */
struct termios {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_cc[20];
};

#define TCSAFLUSH  0
#define BRKINT     0x0001
#define ICRNL      0x0002
#define INPCK      0x0004
#define ISTRIP     0x0008
#define IXON       0x0010
#define OPOST      0x0001
#define CS8        0x0001
#define ECHO       0x0001
#define ICANON     0x0002
#define IEXTEN     0x0004
#define ISIG       0x0008
#define VMIN       0
#define VTIME      1

#ifndef ENOTTY
#define ENOTTY     25
#endif

/* sys/ioctl.h */
struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ 0x5413

/* unistd.h */
#ifndef STDIN_FILENO
#define STDIN_FILENO  0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

/* fcntl.h */
#ifndef O_RDWR
#define O_RDWR   0x0002
#endif
#ifndef O_CREAT
#define O_CREAT  0x0200
#endif

/* signal.h */
#ifndef SIGWINCH
#define SIGWINCH 28
#endif

/* ======================================================================
 * Inline stubs — functions that become no-ops on Bus Pirate
 * ====================================================================== */

/* Terminal is always in raw mode — termios operations are no-ops */
static inline int tcgetattr(int fd, struct termios *t) {
    (void)fd; memset(t, 0, sizeof(*t)); return 0;
}

static inline int tcsetattr(int fd, int opt, const struct termios *t) {
    (void)fd; (void)opt; (void)t; return 0;
}

static inline int isatty(int fd) {
    (void)fd; return 1;
}

/* ioctl — return terminal size from system_config.
 * These values are probed by ui_term_detect() at boot via ESC[6n
 * and are authoritative.  We avoid kilo's built-in probe because
 * stale bytes in the rx_fifo can corrupt the ESC[6n response. */
static inline int kilo_ioctl_impl(int fd, unsigned long req,
                                   struct winsize *ws) {
    (void)fd; (void)req;
    ws->ws_row = system_config.terminal_ansi_rows;
    ws->ws_col = system_config.terminal_ansi_columns;
    return 0;
}
#define ioctl(fd, req, ws) kilo_ioctl_impl((fd), (req), (ws))

/* Signal/atexit — not needed on embedded */
typedef void (*kilo_sighandler_t)(int);
#define signal(sig, handler) ((void)(sig), (kilo_sighandler_t)0)

/* ======================================================================
 * I/O redirection — #define POSIX calls to our implementations
 * ====================================================================== */

/* Terminal read/write (also handles file fd for editorSave) */
ssize_t kilo_io_read(int fd, void *buf, size_t count);
ssize_t kilo_io_write(int fd, const void *buf, size_t count);
#define read  kilo_io_read
#define write kilo_io_write

/* File descriptor I/O (for editorSave: open/ftruncate/write/close) */
int kilo_posix_open(const char *path, int flags, ...);
int kilo_posix_close(int fd);
int kilo_posix_ftruncate(int fd, int length);
#define open      kilo_posix_open
#define close     kilo_posix_close
#define ftruncate kilo_posix_ftruncate

/* stdio FILE I/O (for editorOpen: fopen/getline/fclose) */
FILE *kilo_fopen(const char *path, const char *mode);
int   kilo_fclose(FILE *fp);
ssize_t kilo_getline(char **lineptr, size_t *n, FILE *stream);
#define fopen   kilo_fopen
#define fclose  kilo_fclose
#define getline kilo_getline

/* perror — just print the message (no stderr distinction) */
#define perror(s) printf("%s\n", (s))

/* ======================================================================
 * exit() → longjmp back to edit command handler
 * ====================================================================== */

extern jmp_buf kilo_exit_jmpbuf;

/* Function-like macro: exit(0) → longjmp(buf, 255), exit(n) → longjmp(buf, n)
 * We map 0 to 255 because longjmp(buf, 0) is treated as longjmp(buf, 1) */
#define exit(code) longjmp(kilo_exit_jmpbuf, (code) ? (code) : 255)

/* ======================================================================
 * time() → Pico SDK monotonic timer (seconds since boot)
 * ====================================================================== */

time_t kilo_time(time_t *t);

/* Function-like macro: time(x) matches time(NULL) but not time_t */
#define time(x) kilo_time(x)

/* ======================================================================
 * Arena allocator — all kilo allocations go to BIG_BUFFER (128KB)
 * No system heap usage.  Freed in bulk when the editor exits.
 * ====================================================================== */

void  kilo_arena_init(uint8_t *buf, size_t size);
void *kilo_arena_malloc(size_t size);
void *kilo_arena_realloc(void *ptr, size_t new_size);
void  kilo_arena_free(void *ptr);
size_t kilo_arena_used(void);  /* bytes in use (for status bar) */
size_t kilo_arena_total_size(void);  /* total arena capacity */

/* Redirect all of kilo's dynamic allocation into the arena */
#define malloc  kilo_arena_malloc
#define realloc kilo_arena_realloc
#define free    kilo_arena_free

/* ======================================================================
 * Public API — called from edit.c command handler
 * ====================================================================== */

int  kilo_run(const char *filename);
void kilo_cleanup(void);

#endif /* KILO_COMPAT_H */
