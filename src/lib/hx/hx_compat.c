/*
 * hx_compat.c — POSIX shim implementations for hx hex editor on Bus Pirate 5/6
 *
 * Provides the actual I/O, file, and allocator functions that hx source files
 * call through the #ifdef BUSPIRATE shim function calls.
 */

#include "hx_compat.h"
#include "fatfs/ff.h"

/* ---- Exit mechanism ---- */
jmp_buf hx_exit_jmpbuf;

/* ======================================================================
 * Arena allocator — first-fit with free-block coalescing
 *
 * All of hx's malloc/realloc/free calls are redirected here via
 * macros in hx_compat.h.  The arena is the BIG_BUFFER (128KB)
 * allocated by hexedit.c before calling hx_run().
 *
 * Block layout:  [arena_hdr_t | data ...]
 * Free blocks are coalesced with their neighbour on free().
 * ====================================================================== */

/* Must be undefined before the arena functions — they use raw malloc names */
#undef malloc
#undef realloc
#undef free

#define ARENA_ALIGN 4
#define ARENA_ALIGN_UP(x) (((x) + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1))

typedef struct {
    uint32_t size;   /* data size (aligned) */
    uint32_t used;   /* 1 = in use, 0 = free */
} arena_hdr_t;

#define HDR_SIZE  ARENA_ALIGN_UP(sizeof(arena_hdr_t))

static uint8_t *arena_base;
static size_t   arena_total;

void hx_arena_init(uint8_t *buf, size_t size) {
    arena_base  = buf;
    arena_total = size;
    memset(buf, 0, size);
}

size_t hx_arena_capacity(void) {
    return arena_total;
}

void *hx_arena_malloc(size_t size) {
    if (size == 0) size = 1;
    size = ARENA_ALIGN_UP(size);

    uint8_t *p = arena_base;
    uint8_t *end = arena_base + arena_total;

    while (p + HDR_SIZE <= end) {
        arena_hdr_t *hdr = (arena_hdr_t *)p;

        if (hdr->size == 0) {
            /* Virgin territory — first allocation past all existing blocks */
            if (p + HDR_SIZE + size > end) goto oom;
            hdr->size = size;
            hdr->used = 1;
            return p + HDR_SIZE;
        }
        if (!hdr->used && hdr->size >= size) {
            /* Re-use a freed block */
            if (hdr->size >= size + HDR_SIZE + ARENA_ALIGN) {
                /* Split: create a smaller free block after us */
                arena_hdr_t *rest = (arena_hdr_t *)(p + HDR_SIZE + size);
                rest->size = hdr->size - size - HDR_SIZE;
                rest->used = 0;
                hdr->size = size;
            }
            hdr->used = 1;
            return p + HDR_SIZE;
        }
        p += HDR_SIZE + hdr->size;
    }

oom:
    /* Out of arena memory — bail out of the editor cleanly.
     * Exit code 2 is checked by hexedit_handler to show a message. */
    longjmp(hx_exit_jmpbuf, 2);
    return NULL;  /* unreachable, but satisfies compiler */
}

void hx_arena_free(void *ptr) {
    if (!ptr) return;
    arena_hdr_t *hdr = (arena_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    hdr->used = 0;

    /* Coalesce with the next block if it's also free */
    uint8_t *next_p = (uint8_t *)ptr + hdr->size;
    if (next_p + HDR_SIZE <= arena_base + arena_total) {
        arena_hdr_t *next = (arena_hdr_t *)next_p;
        if (next->size > 0 && !next->used) {
            hdr->size += HDR_SIZE + next->size;
        }
    }
}

void *hx_arena_realloc(void *ptr, size_t new_size) {
    if (!ptr) return hx_arena_malloc(new_size);
    if (new_size == 0) { hx_arena_free(ptr); return NULL; }

    new_size = ARENA_ALIGN_UP(new_size);
    arena_hdr_t *hdr = (arena_hdr_t *)((uint8_t *)ptr - HDR_SIZE);

    /* Already big enough? */
    if (hdr->size >= new_size) return ptr;

    /* Try to grow into an adjacent free block */
    uint8_t *next_p = (uint8_t *)ptr + hdr->size;
    if (next_p + HDR_SIZE <= arena_base + arena_total) {
        arena_hdr_t *next = (arena_hdr_t *)next_p;
        if (next->size > 0 && !next->used) {
            uint32_t combined = hdr->size + HDR_SIZE + next->size;
            if (combined >= new_size) {
                hdr->size = combined;
                /* Split remainder if worthwhile */
                if (hdr->size >= new_size + HDR_SIZE + ARENA_ALIGN) {
                    arena_hdr_t *rest = (arena_hdr_t *)((uint8_t *)ptr + new_size);
                    rest->size = hdr->size - new_size - HDR_SIZE;
                    rest->used = 0;
                    hdr->size = new_size;
                }
                return ptr;
            }
        }
        /* Next block is virgin territory (size==0)? Grow into it. */
        if (next->size == 0) {
            uint8_t *arena_end = arena_base + arena_total;
            if ((uint8_t *)ptr + new_size <= arena_end) {
                hdr->size = new_size;
                return ptr;
            }
        }
    }

    /* Fall back: allocate new, copy, free old */
    void *new_ptr = hx_arena_malloc(new_size);
    /* Note: if hx_arena_malloc hits OOM, it longjmps and never returns.
     * This NULL check is defensive only. */
    if (!new_ptr) longjmp(hx_exit_jmpbuf, 2);
    memcpy(new_ptr, ptr, hdr->size);
    hx_arena_free(ptr);
    return new_ptr;
}

/* Re-enable the macros for the rest of this file */
#define malloc  hx_arena_malloc
#define realloc hx_arena_realloc
#define free    hx_arena_free

/* ======================================================================
 * Terminal I/O — read from USB rx FIFO, write to USB tx FIFO
 * ====================================================================== */

/* ---- vt100_keys shared decoder state ---- */
#include "lib/vt100_keys/vt100_keys.h"

vt100_key_state_t hx_key_state;

static int hx_vt100_read_blocking(char *c) {
    while (!rx_fifo_try_get(c)) {
        tight_loop_contents();
    }
    return 1;
}

static int hx_vt100_read_try(char *c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

void hx_vt100_keys_init(void) {
    vt100_key_init(&hx_key_state, hx_vt100_read_blocking, hx_vt100_read_try);
}

ssize_t hx_io_read(int fd, void *buf, size_t count) {
    (void)fd;
    char *p = (char *)buf;

    for (size_t i = 0; i < count; i++) {
        /* Try to read one byte with ~100ms timeout (for escape sequences).
         * The main read loop in read_key() retries on 0, so this
         * effectively blocks until a key is pressed. */
        absolute_time_t deadline = make_timeout_time_ms(100);
        bool got = false;
        while (!time_reached(deadline)) {
            if (rx_fifo_try_get(&p[i])) {
                got = true;
                break;
            }
            tight_loop_contents();
        }
        if (!got) return (ssize_t)i;   /* timeout — return bytes read so far */
    }
    return (ssize_t)count;
}

ssize_t hx_io_write(int fd, const void *buf, size_t count) {
    (void)fd;
    /* All hx output goes to the terminal (USB CDC) */
    tx_fifo_write((const char *)buf, count);
    return (ssize_t)count;
}

/* ======================================================================
 * Terminal size — from system_config
 * ====================================================================== */

int hx_get_window_size(int *rows, int *cols) {
    *rows = system_config.terminal_ansi_rows;
    *cols = system_config.terminal_ansi_columns;
    return 0;
}

/* ======================================================================
 * File I/O — FatFS wrappers
 * ====================================================================== */

static FIL hx_read_fil;   /* static FIL for read operations */

int hx_file_open_read(const char *path, FSIZE_t *out_size) {
    FRESULT res = f_open(&hx_read_fil, path, FA_READ);
    if (res != FR_OK) {
        return -(int)res;  /* negative FRESULT code */
    }
    *out_size = f_size(&hx_read_fil);
    return 0;
}

int hx_file_read(void *buf, size_t count) {
    UINT br;
    FRESULT res = f_read(&hx_read_fil, buf, count, &br);
    if (res != FR_OK) {
        return -1;
    }
    return (int)br;
}

void hx_file_close_read(void) {
    f_close(&hx_read_fil);
}

int hx_file_write_all(const char *path, const void *buf, size_t count) {
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        return -1;
    }

    UINT bw;
    res = f_write(&fil, buf, count, &bw);
    f_close(&fil);

    if (res != FR_OK || bw != count) {
        return -1;
    }
    return 0;
}

int hx_file_read_at(const char *path, unsigned int offset, void *buf, size_t count) {
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res == FR_NO_FILE || res == FR_NO_PATH) return -2;
    if (res != FR_OK) return -1;

    if (offset > 0) {
        res = f_lseek(&fil, (FSIZE_t)offset);
        if (res != FR_OK) { f_close(&fil); return -1; }
    }

    UINT br;
    res = f_read(&fil, buf, count, &br);
    f_close(&fil);
    if (res != FR_OK) return -1;
    return (int)br;
}
