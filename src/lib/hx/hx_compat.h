/*
 * hx_compat.h — POSIX compatibility shim for hx hex editor on Bus Pirate 5/6
 *
 * This header provides stub types, constants, and #define macros that redirect
 * POSIX I/O calls to Bus Pirate platform functions (USB FIFO, FatFS, etc).
 *
 * Unlike kilo (single .c file, unity build), hx has multiple source files.
 * Each upstream .c file that touches I/O has minimal #ifdef BUSPIRATE blocks
 * that call the shim functions declared here.  This header is included by:
 *   - editor.c   (file I/O via hx_file_* functions, exit → longjmp)
 *   - util.c     (terminal read/write via hx_io_*, window size)
 *   - charbuf.c  (terminal write via hx_io_write)
 *
 * The remaining upstream files (undo.c, undo.h, editor.h, charbuf.h) are
 * pristine and don't need this header.
 */

#ifndef HX_COMPAT_H
#define HX_COMPAT_H

/* ---- Standard C library (available in Pico SDK newlib) ---- */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

/* ---- Pico SDK ---- */
#include "pico/stdlib.h"

/* ---- Bus Pirate platform ---- */
#include "pirate.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"

/* ---- FatFS (for FSIZE_t used in editor.c) ---- */
#include "fatfs/ff.h"

/* ======================================================================
 * Version string (upstream defines this in Makefile/cmake)
 * ====================================================================== */

#ifndef HX_VERSION
#define HX_VERSION "0.0.1-bp"
#endif

/* ======================================================================
 * Terminal I/O — read from USB rx FIFO, write to USB tx FIFO
 * ====================================================================== */

ssize_t hx_io_read(int fd, void *buf, size_t count);
ssize_t hx_io_write(int fd, const void *buf, size_t count);

/* ---- vt100_keys shared decoder ---- */
#include "lib/vt100_keys/vt100_keys.h"
extern vt100_key_state_t hx_key_state;
void hx_vt100_keys_init(void);

/* ======================================================================
 * Terminal size — from system_config (probed at boot via ESC[6n)
 * ====================================================================== */

int hx_get_window_size(int *rows, int *cols);

/* ======================================================================
 * File I/O — FatFS wrappers for editor_openfile / editor_writefile
 *
 * hx uses a different file I/O pattern from kilo:
 *   - openfile:  fopen → stat → fread → fclose  (bulk read)
 *   - writefile: fopen → fwrite → fclose         (bulk write)
 *
 * We provide simple open/read/close/write_all functions so editor.c
 * can use them under #ifdef BUSPIRATE without touching the POSIX paths.
 * ====================================================================== */

/* Open file for reading. Returns 0 on success, -1 on error, -2 if not found.
 * On success, *out_size is set to the file size. */
int hx_file_open_read(const char *path, FSIZE_t *out_size);

/* Read up to `count` bytes from the currently open read file.
 * Returns bytes read, or -1 on error. */
int hx_file_read(void *buf, size_t count);

/* Close the read file. */
void hx_file_close_read(void);

/* Write `count` bytes from `buf` to `path`, creating/truncating as needed.
 * Returns 0 on success, -1 on error. */
int hx_file_write_all(const char *path, const void *buf, size_t count);

/* ======================================================================
 * exit() → longjmp back to hexedit command handler
 * ====================================================================== */

extern jmp_buf hx_exit_jmpbuf;

/* Function-like macro: exit(0) → longjmp(buf, 255), exit(n) → longjmp(buf, n)
 * We map 0 to 255 because longjmp(buf, 0) is treated as longjmp(buf, 1) */
#define exit(code) longjmp(hx_exit_jmpbuf, (code) ? (code) : 255)

/* ======================================================================
 * Arena allocator — all hx allocations go to BIG_BUFFER (128KB)
 * No system heap usage.  Freed in bulk when the editor exits.
 * ====================================================================== */

void  hx_arena_init(uint8_t *buf, size_t size);
void *hx_arena_malloc(size_t size);
void *hx_arena_realloc(void *ptr, size_t new_size);
void  hx_arena_free(void *ptr);

/* Redirect all of hx's dynamic allocation into the arena */
#define malloc  hx_arena_malloc
#define realloc hx_arena_realloc
#define free    hx_arena_free

/* perror — just print the message (no stderr distinction) */
#undef perror
#define perror(s) printf("%s\n", (s))

/* fprintf(stderr, ...) — redirect to printf */
#define fprintf(stream, ...) printf(__VA_ARGS__)

/* ======================================================================
 * Public API — called from hexedit.c command handler
 * ====================================================================== */

int  hx_run(const char *filename);
void hx_cleanup(void);

#endif /* HX_COMPAT_H */
