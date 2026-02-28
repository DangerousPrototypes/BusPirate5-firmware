/*
 * kilo_compat.c — POSIX shim implementations for kilo on Bus Pirate 5/6
 *
 * Provides the actual I/O, file, and timer functions that kilo.c calls
 * through the #define redirections in kilo_compat.h.
 */

#include "kilo_compat.h"
#include "fatfs/ff.h"

/* ---- Exit mechanism ---- */
jmp_buf kilo_exit_jmpbuf;

/* ======================================================================
 * Arena allocator — first-fit with free-block coalescing
 *
 * All of kilo's malloc/realloc/free calls are redirected here via
 * macros in kilo_compat.h.  The arena is the BIG_BUFFER (128KB)
 * allocated by edit.c before calling kilo_run().
 *
 * Block layout:  [arena_hdr_t | data ...]
 * Free blocks are coalesced with their neighbour on free().
 * ====================================================================== */

/* Must be defined before the malloc macro takes effect */
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

void kilo_arena_init(uint8_t *buf, size_t size) {
    arena_base  = buf;
    arena_total = size;
    memset(buf, 0, size);
}

void *kilo_arena_malloc(size_t size) {
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
     * Exit code 2 is checked by edit_handler to show a message. */
    longjmp(kilo_exit_jmpbuf, 2);
    return NULL;  /* unreachable, but satisfies compiler */
}

void kilo_arena_free(void *ptr) {
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

void *kilo_arena_realloc(void *ptr, size_t new_size) {
    if (!ptr) return kilo_arena_malloc(new_size);
    if (new_size == 0) { kilo_arena_free(ptr); return NULL; }

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
    void *new_ptr = kilo_arena_malloc(new_size);
    /* Note: if kilo_arena_malloc hits OOM, it longjmps and never returns.
     * This NULL check is defensive only. */
    if (!new_ptr) longjmp(kilo_exit_jmpbuf, 2);
    memcpy(new_ptr, ptr, hdr->size);
    kilo_arena_free(ptr);
    return new_ptr;
}

/* Re-enable the macros for the rest of this file (getline uses them) */
#define malloc  kilo_arena_malloc
#define realloc kilo_arena_realloc
#define free    kilo_arena_free

size_t kilo_arena_used(void) {
    size_t used = 0;
    uint8_t *p = arena_base;
    uint8_t *end = arena_base + arena_total;
    while (p + HDR_SIZE <= end) {
        arena_hdr_t *hdr = (arena_hdr_t *)p;
        if (hdr->size == 0) break;  /* end of allocated region */
        if (hdr->used) used += hdr->size;
        used += HDR_SIZE;  /* header overhead counts too */
        p += HDR_SIZE + hdr->size;
    }
    return used;
}

size_t kilo_arena_total_size(void) {
    return arena_total;
}

/* ======================================================================
 * Terminal I/O — read from USB rx FIFO, write to USB tx FIFO
 * ====================================================================== */

/* ---- vt100_keys shared decoder state ---- */
#include "lib/vt100_keys/vt100_keys.h"

vt100_key_state_t kilo_key_state;

static int kilo_vt100_read_blocking(char *c) {
    while (!rx_fifo_try_get(c)) {
        tight_loop_contents();
    }
    return 1;
}

static int kilo_vt100_read_try(char *c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

void kilo_vt100_keys_init(void) {
    vt100_key_init(&kilo_key_state, kilo_vt100_read_blocking, kilo_vt100_read_try);
}

/* Sentinel fd returned by kilo_posix_open for file writes */
#define KILO_FILE_FD  42

ssize_t kilo_io_read(int fd, void *buf, size_t count) {
    (void)fd;
    char *p = (char *)buf;

    for (size_t i = 0; i < count; i++) {
        /* Try to read one byte with ~100ms timeout (for escape sequences).
         * The main read loop in editorReadKey retries on 0, so this
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

static FIL kilo_save_fil;   /* single static FIL for save operations */

ssize_t kilo_io_write(int fd, const void *buf, size_t count) {
    if (fd == STDOUT_FILENO || fd == 1) {
        /* Terminal output */
        tx_fifo_write((const char *)buf, count);
        return (ssize_t)count;
    }
    if (fd == KILO_FILE_FD) {
        /* File write via FatFS */
        UINT bw;
        FRESULT res = f_write(&kilo_save_fil, buf, count, &bw);
        if (res != FR_OK) return -1;
        return (ssize_t)bw;
    }
    return -1;
}

/* ======================================================================
 * File descriptor I/O — for editorSave (open/ftruncate/write/close)
 * ====================================================================== */

int kilo_posix_open(const char *path, int flags, ...) {
    (void)flags;
    /* kilo always opens for save with O_RDWR|O_CREAT then ftruncate+write.
     * FA_CREATE_ALWAYS truncates to 0 which achieves the same effect. */
    FRESULT res = f_open(&kilo_save_fil, path,
                         FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) return -1;
    return KILO_FILE_FD;
}

int kilo_posix_close(int fd) {
    if (fd == KILO_FILE_FD) {
        f_close(&kilo_save_fil);
    }
    return 0;
}

int kilo_posix_ftruncate(int fd, int length) {
    (void)fd; (void)length;
    /* FA_CREATE_ALWAYS already truncated on open. No-op. */
    return 0;
}

/* ======================================================================
 * FILE I/O — for editorOpen (fopen/getline/fclose)
 * ====================================================================== */

static FIL kilo_read_fil;   /* single static FIL for read operations */

FILE *kilo_fopen(const char *path, const char *mode) {
    BYTE fm;
    if (mode[0] == 'r')
        fm = FA_READ;
    else
        fm = FA_WRITE | FA_CREATE_ALWAYS;

    if (f_open(&kilo_read_fil, path, fm) != FR_OK) {
        errno = ENOENT;  /* let kilo treat this as "new file" */
        return NULL;
    }

    /* Return a non-NULL sentinel. kilo only checks for NULL and passes
     * this pointer to our shimmed getline/fclose — never to real stdio. */
    return (FILE *)(uintptr_t)&kilo_read_fil;
}

int kilo_fclose(FILE *fp) {
    (void)fp;
    f_close(&kilo_read_fil);
    return 0;
}

/*
 * POSIX getline() replacement using FatFS f_read().
 * Reads one line (up to '\n') into a malloc'd buffer.
 * Returns line length on success, -1 on EOF/error.
 */
ssize_t kilo_getline(char **lineptr, size_t *n, FILE *stream) {
    (void)stream;

    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (!*lineptr) return -1;
    }

    size_t pos = 0;
    char c;
    UINT br;

    while (1) {
        FRESULT res = f_read(&kilo_read_fil, &c, 1, &br);
        if (res != FR_OK || br == 0) {
            /* EOF or error */
            if (pos == 0) return -1;
            break;
        }

        /* Grow buffer if needed */
        if (pos + 2 >= *n) {
            *n *= 2;
            char *newp = realloc(*lineptr, *n);
            if (!newp) return -1;
            *lineptr = newp;
        }

        (*lineptr)[pos++] = c;
        if (c == '\n') break;
    }

    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}

/* ======================================================================
 * Timer — seconds since boot (for status message timeout)
 * ====================================================================== */

time_t kilo_time(time_t *t) {
    time_t sec = (time_t)(to_ms_since_boot(get_absolute_time()) / 1000);
    if (t) *t = sec;
    return sec;
}
