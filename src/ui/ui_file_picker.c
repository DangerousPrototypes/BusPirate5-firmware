/**
 * @file ui_file_picker.c
 * @brief Reusable fullscreen VT100 file picker with scrollable list.
 *
 * Presents a fullscreen scrollable file list with highlighted selection bar.
 * Features:
 *   - Arrow key / PageUp / PageDown scrolling through all files
 *   - Styled centered popup for entering new filenames
 *   - File sizes shown alongside names
 *   - Extension filtering
 *   - Both standalone and embedded modes
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_file_picker.h"
#include "ui/ui_popup.h"
#include "ui/ui_app.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "fatfs/ff.h"
#include "lib/vt100_keys/vt100_keys.h"

/* ── File entry storage ─────────────────────────────────────────────── */

typedef struct {
    char name[13];      /* 8.3 filename */
    char size_str[8];   /* formatted size: "256B", "1.2K", etc. */
} file_entry_t;

/* ── Size formatting ────────────────────────────────────────────────── */

static void fp_format_size(uint32_t bytes, char* buf, uint8_t buf_size) {
    if (bytes < 1024) {
        snprintf(buf, buf_size, "%luB", (unsigned long)bytes);
    } else if (bytes < 1024 * 1024) {
        uint32_t kb = bytes / 1024;
        uint32_t frac = ((bytes % 1024) * 10) / 1024;
        if (frac > 0 && kb < 100) {
            snprintf(buf, buf_size, "%lu.%luK", (unsigned long)kb, (unsigned long)frac);
        } else {
            snprintf(buf, buf_size, "%luK", (unsigned long)kb);
        }
    } else {
        uint32_t mb = bytes / (1024 * 1024);
        snprintf(buf, buf_size, "%luM", (unsigned long)mb);
    }
}

/* ── Extension filter & streaming helpers ──────────────────────────── */

static const char* fp_ext_filter; /* set by ui_file_pick before any scan */

static bool fp_entry_matches(const FILINFO* fno) {
    if (fno->fattrib & (AM_DIR | AM_HID | AM_SYS)) return false;
    if (!fp_ext_filter || !fp_ext_filter[0]) return true;
    const char* dot = strrchr(fno->fname, '.');
    if (!dot) return false;
    dot++;
    int i = 0;
    for (; fp_ext_filter[i] && dot[i]; i++) {
        char a = fp_ext_filter[i], b = dot[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return fp_ext_filter[i] == 0 && dot[i] == 0;
}

/* Count matching files — opens dir, scans, closes. No buffer needed. */
static uint8_t fp_count_files(void) {
    DIR dir;
    FILINFO fno;
    uint8_t count = 0;
    if (f_opendir(&dir, "") != FR_OK) return 0;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (fp_entry_matches(&fno)) count++;
    }
    f_closedir(&dir);
    return count;
}

/* Copy the name of the idx-th matching file (0-based) into buf. */
static bool fp_read_entry_name(uint8_t idx, char* buf, uint8_t buf_size) {
    DIR dir;
    FILINFO fno;
    uint8_t seen = 0;
    bool found = false;
    if (f_opendir(&dir, "") != FR_OK) return false;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (!fp_entry_matches(&fno)) continue;
        if (seen == idx) {
            strncpy(buf, fno.fname, buf_size - 1);
            buf[buf_size - 1] = 0;
            found = true;
            break;
        }
        seen++;
    }
    f_closedir(&dir);
    return found;
}

/* ── I/O abstraction ────────────────────────────────────────────────── */

/* Active I/O callbacks — set at entry, used by helpers */
static int (*fp_write_fn)(int, const void*, int);
static int (*fp_read_key_fn)(void);
static void (*fp_repaint_fn)(void);
static uint8_t fp_cols, fp_rows;

static void fp_write_str(const char* s) {
    fp_write_fn(0, s, (int)strlen(s));
}

static void fp_write_buf(const char* buf, int len) {
    fp_write_fn(0, buf, len);
}

/* ── Draw helpers ───────────────────────────────────────────────────── */

/* VT100 attribute strings matching the menu framework palette */
#define FP_ATTR_TITLE    "\x1b[1;37;44m"   /* bold white on blue (title bar) */
#define FP_ATTR_BORDER   "\x1b[0;36m"      /* cyan (box border) */
#define FP_ATTR_NORMAL   "\x1b[0m"         /* default */
#define FP_ATTR_SELECTED "\x1b[1;37;44m"   /* bold white on blue (highlight) */
#define FP_ATTR_SIZE     "\x1b[0;33m"      /* yellow (size column) */
#define FP_ATTR_SIZE_SEL "\x1b[0;33;44m"   /* yellow on blue (size in selected row) */
#define FP_ATTR_HINT     "\x1b[0;2m"       /* dim (hints/empty) */
#define FP_ATTR_STATUS   "\x1b[0;30;47m"   /* black on white (status bar) */
#define FP_ATTR_NEW_FILE "\x1b[0;32m"      /* green (new file entry) */
#define FP_ATTR_NEW_SEL  "\x1b[0;32;44m"   /* green on blue (new file selected) */

/* ── Scroll elevator characters ────────────────────────────────────── */
/* Unicode block elements — 3 UTF-8 bytes each, 1 column wide */
#define FP_SCROLLBAR_THUMB  "\xe2\x96\x93"  /* ▓ (U+2593) */
#define FP_SCROLLBAR_TRACK  "\xe2\x96\x91"  /* ░ (U+2591) */
/* ASCII fallback — uncomment these and comment out the two lines above:
   #define FP_SCROLLBAR_THUMB  "#"
   #define FP_SCROLLBAR_TRACK  "|"  */

/* ── Grid geometry constants ────────────────────────────────────────── */
#define FP_COL_MIN_WIDTH   20   /* minimum chars per grid column (name + size) */
#define FP_COL_NAME_WIDTH  12   /* max 8.3 filename length */
#define FP_MAX_COLS         8   /* max columns; sizes the per-row stack buffer */

static void fp_goto(int row, int col) {
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    fp_write_buf(buf, n);
}

/**
 * Draw the fullscreen file picker.
 *
 * Streams matching entries from FatFS on each call — no bulk buffer.
 * Only the current visible row's entries (≤ FP_MAX_COLS) are held on stack.
 *
 * @param num_cols        Number of file grid columns (pre-computed by caller).
 * @param total_grid_rows Total file grid rows = ceil(file_count / num_cols).
 */
static void fp_draw_screen(uint8_t file_count, uint8_t cursor,
                           uint8_t scroll_top_row,
                           int num_cols, int total_grid_rows) {
    char buf[160];
    int n;

    fp_write_str("\x1b[?25l");  /* hide cursor */

    /* ── Geometry ── */
    int left_col    = 2;
    int inner_width = (int)fp_cols - 2;
    if (inner_width < 22) inner_width = 22;

    /* Content area: inner_width - 2 borders - 1 elevator column */
    int content_width = inner_width - 3;
    if (content_width < 18) content_width = 18;

    int col_width = (num_cols > 0) ? (content_width / num_cols) : content_width;
    if (col_width < 1) col_width = 1;

    int list_top    = 4;           /* first grid row */
    int list_height = (int)fp_rows - 5;
    if (list_height < 2) list_height = 2;

    /* ── Scroll elevator ── */
    bool need_scroll = (total_grid_rows > list_height);
    int thumb_size  = list_height;
    int thumb_start = 0;
    if (need_scroll) {
        thumb_size = (list_height * list_height + total_grid_rows - 1) / total_grid_rows;
        if (thumb_size < 1) thumb_size = 1;
        if (thumb_size > list_height) thumb_size = list_height;
        int max_scroll = total_grid_rows - list_height;
        if (max_scroll > 0)
            thumb_start = (int)scroll_top_row * (list_height - thumb_size) / max_scroll;
    }

    /* ── Row 1: title bar ── */
    fp_goto(1, 1);
    fp_write_str(FP_ATTR_TITLE);
    n = snprintf(buf, sizeof(buf), " Select File");
    fp_write_buf(buf, n);
    for (int i = n - 1; i < (int)fp_cols; i++) fp_write_str(" ");

    /* ── Row 2: top border ── */
    fp_goto(2, left_col);
    fp_write_str(FP_ATTR_BORDER "+");
    for (int i = 0; i < inner_width - 2; i++) fp_write_str("-");
    fp_write_str("+" FP_ATTR_NORMAL "\x1b[K");

    /* ── Row 3: "New file..." — pinned, always visible ── */
    {
        bool is_sel = (cursor == 0);
        fp_goto(3, left_col);
        fp_write_str(FP_ATTR_BORDER "|");
        fp_write_str(is_sel ? FP_ATTR_NEW_SEL : FP_ATTR_NEW_FILE);
        const char* label = " [Enter filename...]";
        int llen = (int)strlen(label);
        fp_write_str(label);
        for (int p = llen; p < content_width; p++) fp_write_str(" ");
        fp_write_str(FP_ATTR_NORMAL " " FP_ATTR_BORDER "|" FP_ATTR_NORMAL "\x1b[K");
    }

    /* ── Rows 4..list_top+list_height-1: file grid — streamed from FatFS ── */
    {
        file_entry_t row_buf[FP_MAX_COLS];   /* ≤ FP_MAX_COLS * 21 B on stack */
        DIR dir;
        FILINFO fno;
        bool dir_ok = (f_opendir(&dir, "") == FR_OK);

        /* Advance dir stream to first visible entry */
        if (dir_ok) {
            uint32_t skip = (uint32_t)scroll_top_row * (uint32_t)num_cols;
            uint32_t skipped = 0;
            while (skipped < skip) {
                if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
                    f_closedir(&dir);
                    dir_ok = false;
                    break;
                }
                if (fp_entry_matches(&fno)) skipped++;
            }
        }

        for (int vis = 0; vis < list_height; vis++) {
            int grid_row = (int)scroll_top_row + vis;
            int row      = list_top + vis;

            /* Elevator character for this vis position */
            const char* elev;
            if (!need_scroll) {
                elev = " ";
            } else if (vis >= thumb_start && vis < thumb_start + thumb_size) {
                elev = FP_SCROLLBAR_THUMB;
            } else {
                elev = FP_SCROLLBAR_TRACK;
            }

            fp_goto(row, left_col);
            fp_write_str(FP_ATTR_BORDER "|");

            /* Fill row_buf with up to num_cols entries from the dir stream */
            uint8_t row_count = 0;
            if (dir_ok && grid_row < total_grid_rows) {
                while (row_count < (uint8_t)num_cols) {
                    if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
                        f_closedir(&dir);
                        dir_ok = false;
                        break;
                    }
                    if (!fp_entry_matches(&fno)) continue;
                    strncpy(row_buf[row_count].name, fno.fname, 12);
                    row_buf[row_count].name[12] = 0;
                    fp_format_size((uint32_t)fno.fsize,
                                   row_buf[row_count].size_str,
                                   sizeof(row_buf[row_count].size_str));
                    row_count++;
                }
            }

            int chars_written = 0;
            for (int gc = 0; gc < (int)row_count; gc++) {
                uint8_t item_idx = (uint8_t)(grid_row * num_cols + gc);
                bool is_selected = ((uint8_t)(item_idx + 1) == cursor);
                fp_write_str(is_selected ? FP_ATTR_SELECTED : FP_ATTR_NORMAL);

                int name_len = (int)strlen(row_buf[gc].name);
                if (name_len > FP_COL_NAME_WIDTH) name_len = FP_COL_NAME_WIDTH;

                fp_write_str(" ");
                fp_write_buf(row_buf[gc].name, name_len);
                int cell_used = 1 + name_len;

                /* Size right-aligned within the cell, if it fits */
                int size_len = (int)strlen(row_buf[gc].size_str);
                int gap = col_width - cell_used - size_len - 1;  /* 1 trailing space */
                if (gap >= 0) {
                    for (int p = 0; p < gap; p++) fp_write_str(" ");
                    fp_write_str(is_selected ? FP_ATTR_SIZE_SEL : FP_ATTR_SIZE);
                    fp_write_buf(row_buf[gc].size_str, size_len);
                    fp_write_str(is_selected ? FP_ATTR_SELECTED : FP_ATTR_NORMAL);
                    fp_write_str(" ");
                    cell_used += gap + size_len + 1;
                }

                /* Pad cell to col_width */
                fp_write_str(FP_ATTR_NORMAL);
                for (; cell_used < col_width; cell_used++) fp_write_str(" ");
                chars_written += col_width;
            }

            /* Pad any remainder (empty cells or integer-division slack) */
            fp_write_str(FP_ATTR_NORMAL);
            for (; chars_written < content_width; chars_written++) fp_write_str(" ");

            /* Elevator column + right border */
            fp_write_str(FP_ATTR_HINT);
            fp_write_str(elev);
            fp_write_str(FP_ATTR_BORDER "|" FP_ATTR_NORMAL "\x1b[K");
        }

        if (dir_ok) f_closedir(&dir);
    }

    /* ── Bottom border ── */
    fp_goto(list_top + list_height, left_col);
    fp_write_str(FP_ATTR_BORDER "+");
    for (int i = 0; i < inner_width - 2; i++) fp_write_str("-");
    fp_write_str("+" FP_ATTR_NORMAL "\x1b[K");

    /* ── Status bar ── */
    {
        char pos_buf[12];
        if (cursor > 0) {
            snprintf(pos_buf, sizeof(pos_buf), " %d/%d", (int)cursor, (int)file_count);
        } else {
            snprintf(pos_buf, sizeof(pos_buf), " [new]");
        }
        n = snprintf(buf, sizeof(buf),
                     "%s  |  " UI_HINT_UPDOWN_SELECT_CANCEL "  LR=col",
                     pos_buf);
        fp_goto((int)fp_rows, 1);
        fp_write_str(FP_ATTR_STATUS);
        fp_write_buf(buf, n);
        for (int i = n; i < (int)fp_cols; i++) fp_write_str(" ");
    }

    fp_write_str(FP_ATTR_NORMAL);
}

/* ── Styled popup for filename input (delegates to ui_popup) ────────── */

static bool fp_popup_filename(char* file_buf, uint8_t buf_size) {
    ui_popup_io_t pio = {
        .write_out = fp_write_fn,
        .cols = fp_cols,
        .rows = fp_rows,
    };
    return ui_popup_text_input(&pio, "Enter filename (8.3)", NULL,
                               file_buf, buf_size > 13 ? 13 : buf_size,
                               UI_INPUT_PRINT);
}

/* ── Standalone app scaffold ────────────────────────────────────────── */

static ui_app_t fp_standalone_app;

static int fp_standalone_read_key(void) {
    return ui_app_read_key(&fp_standalone_app);
}

/* ── Public API ─────────────────────────────────────────────────────── */

bool ui_file_pick(const char* ext,
                  char* file_buf,
                  uint8_t buf_size,
                  const ui_file_picker_io_t* io) {
    /* Set up extension filter then count matching files */
    fp_ext_filter = ext;
    uint8_t file_count = fp_count_files();

    /* Set up I/O — either from caller or standalone */
    bool standalone = (io == NULL);

    if (standalone) {
        ui_app_open(&fp_standalone_app);
        fp_cols        = fp_standalone_app.cols;
        fp_rows        = fp_standalone_app.rows;
        fp_read_key_fn = fp_standalone_read_key;
        fp_write_fn    = ui_app_write_out_cb;
        fp_repaint_fn  = NULL;
    } else {
        fp_cols        = io->cols;
        fp_rows        = io->rows;
        fp_read_key_fn = io->read_key;
        fp_write_fn    = io->write_out;
        fp_repaint_fn  = io->repaint;
    }

    /* Grid geometry — computed once from terminal dimensions */
    int inner_width_g   = (int)fp_cols - 2;
    if (inner_width_g < 22) inner_width_g = 22;
    int content_width_g = inner_width_g - 3;
    if (content_width_g < 18) content_width_g = 18;
    int num_cols        = content_width_g / FP_COL_MIN_WIDTH;
    if (num_cols < 1) num_cols = 1;
    if (num_cols > FP_MAX_COLS) num_cols = FP_MAX_COLS;
    int total_grid_rows = (file_count > 0) ? ((int)file_count + num_cols - 1) / num_cols : 0;

    /* Navigation state */
    uint8_t cursor         = (file_count > 0) ? 1 : 0;
    uint8_t scroll_top_row = 0;
    int     list_height    = (int)fp_rows - 5;
    if (list_height < 2) list_height = 2;

    bool selected = false;
    bool running  = true;

    /* Initial draw */
    fp_draw_screen(file_count, cursor, scroll_top_row, num_cols, total_grid_rows);

    while (running) {
        int key = fp_read_key_fn();

        switch (key) {
        case VT100_KEY_UP:
            if (cursor > 0) {
                if (cursor <= (uint8_t)num_cols) {
                    /* On first grid row — go to new-file row */
                    cursor = 0;
                    scroll_top_row = 0;
                } else {
                    cursor -= (uint8_t)num_cols;
                    int gr = ((int)cursor - 1) / num_cols;
                    if (gr < (int)scroll_top_row) scroll_top_row = (uint8_t)gr;
                }
            }
            break;

        case VT100_KEY_DOWN:
            if (cursor == 0) {
                if (file_count > 0) {
                    cursor = 1;
                    scroll_top_row = 0;
                }
            } else {
                uint8_t nc = cursor + (uint8_t)num_cols;
                if (nc <= file_count) {
                    cursor = nc;
                    int gr = ((int)cursor - 1) / num_cols;
                    if (gr >= (int)scroll_top_row + list_height)
                        scroll_top_row = (uint8_t)(gr - list_height + 1);
                }
            }
            break;

        case VT100_KEY_LEFT:
            if (cursor > 0) {
                int gc = ((int)cursor - 1) % num_cols;
                if (gc > 0) {
                    cursor--;
                    int gr = ((int)cursor - 1) / num_cols;
                    if (gr < (int)scroll_top_row) scroll_top_row = (uint8_t)gr;
                }
            }
            break;

        case VT100_KEY_RIGHT:
            if (cursor > 0 && cursor < file_count) {
                int gc = ((int)cursor - 1) % num_cols;
                if (gc < num_cols - 1) {
                    cursor++;
                    int gr = ((int)cursor - 1) / num_cols;
                    if (gr >= (int)scroll_top_row + list_height)
                        scroll_top_row = (uint8_t)(gr - list_height + 1);
                }
            }
            break;

        case VT100_KEY_PAGEUP: {
            if (cursor > 0) {
                int new_top = (int)scroll_top_row - list_height;
                if (new_top < 0) new_top = 0;
                scroll_top_row = (uint8_t)new_top;
                int gr = ((int)cursor - 1) / num_cols;
                if (gr < (int)scroll_top_row) {
                    int item = (int)scroll_top_row * num_cols;
                    cursor = (item < (int)file_count) ? (uint8_t)(item + 1) : file_count;
                }
            }
            break;
        }

        case VT100_KEY_PAGEDOWN: {
            if (cursor == 0 && file_count > 0) cursor = 1;
            if (cursor > 0) {
                int max_top = total_grid_rows - list_height;
                if (max_top < 0) max_top = 0;
                int new_top = (int)scroll_top_row + list_height;
                if (new_top > max_top) new_top = max_top;
                scroll_top_row = (uint8_t)new_top;
                int gr = ((int)cursor - 1) / num_cols;
                if (gr < (int)scroll_top_row) {
                    int item = (int)scroll_top_row * num_cols;
                    cursor = (item < (int)file_count) ? (uint8_t)(item + 1) : file_count;
                }
            }
            break;
        }

        case VT100_KEY_HOME:
            cursor = 0;
            scroll_top_row = 0;
            break;

        case VT100_KEY_END:
            cursor = file_count;
            if (total_grid_rows > list_height) {
                scroll_top_row = (uint8_t)(total_grid_rows - list_height);
            } else {
                scroll_top_row = 0;
            }
            break;

        case VT100_KEY_ENTER:
        case '\n':
            if (cursor == 0) {
                /* "New file..." — show popup; only exit picker on confirm */
                selected = fp_popup_filename(file_buf, buf_size);
                if (selected) running = false;
            } else {
                /* Pick existing file — read name directly from FatFS */
                if (fp_read_entry_name(cursor - 1, file_buf, buf_size)) {
                    selected = true;
                    running = false;
                }
            }
            break;

        case VT100_KEY_ESC:
        case VT100_KEY_CTRL_Q:
            running = false;
            break;

        default:
            break;
        }

        if (running) {
            fp_draw_screen(file_count, cursor, scroll_top_row, num_cols, total_grid_rows);
        }
    }

    /* Cleanup standalone mode */
    if (standalone) {
        ui_app_close(&fp_standalone_app);
    }

    return selected;
}
