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
    char name[13];      /* 8.3 filename, or ".." */
    char size_str[8];   /* formatted size: "256B", "1.2K", etc. */
    bool is_dir;        /* true for directories (including "..") */
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

/* ── CWD + extension filter ─────────────────────────────────────────── */

static const char* fp_ext_filter; /* set by ui_file_pick before any scan */
static char fp_cwd[128];          /* "/" at root, "/SUBDIR" in subdirectory */

static bool fp_not_at_root(void) {
    return fp_cwd[1] != '\0';
}

static void fp_descend(const char* name) {
    size_t clen = strlen(fp_cwd);
    if (fp_cwd[clen - 1] != '/') {
        if (clen + 1 < sizeof(fp_cwd)) { fp_cwd[clen++] = '/'; fp_cwd[clen] = '\0'; }
    }
    strncat(fp_cwd, name, sizeof(fp_cwd) - strlen(fp_cwd) - 1);
}

static void fp_ascend(void) {
    char* last = strrchr(fp_cwd, '/');
    if (last && last != fp_cwd) { *last = '\0'; }
    else { fp_cwd[0] = '/'; fp_cwd[1] = '\0'; }
}

/* ── Entry visibility ───────────────────────────────────────────────── */

/* Directories always visible (unless hidden/system); files filtered by ext. */
static bool fp_entry_visible(const FILINFO* fno) {
    if (fno->fattrib & (AM_HID | AM_SYS)) return false;
    if (fno->fattrib & AM_DIR) return true;
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

/* Count all visible entries including synthetic ".." when in a subdirectory. */
static uint16_t fp_count_entries(void) {
    DIR dir;
    FILINFO fno;
    uint16_t count = fp_not_at_root() ? 1 : 0; /* synthetic ".." */
    if (f_opendir(&dir, fp_cwd) != FR_OK) return count;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (fp_entry_visible(&fno)) count++;
    }
    f_closedir(&dir);
    return count;
}

/* ── Entry info ─────────────────────────────────────────────────────── */

typedef struct {
    char name[13];
    bool is_dir;
} fp_entry_info_t;

/* Read the idx-th visible entry (0-based).  Index 0 inside a subdirectory
 * is always the synthetic ".." item; FatFS indices are adjusted accordingly. */
static bool fp_read_entry_info(uint16_t idx, fp_entry_info_t* out) {
    if (fp_not_at_root()) {
        if (idx == 0) {
            out->name[0] = '.'; out->name[1] = '.'; out->name[2] = '\0';
            out->is_dir = true;
            return true;
        }
        idx--; /* adjust past the synthetic ".." */
    }
    DIR dir;
    FILINFO fno;
    uint16_t seen = 0;
    bool found = false;
    if (f_opendir(&dir, fp_cwd) != FR_OK) return false;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (!fp_entry_visible(&fno)) continue;
        if (seen == idx) {
            strncpy(out->name, fno.fname, 12);
            out->name[12] = '\0';
            out->is_dir = (fno.fattrib & AM_DIR) != 0;
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
#define FP_ATTR_DIR      "\x1b[0;36m"      /* cyan (directory) */
#define FP_ATTR_DIR_SEL  "\x1b[0;36;44m"   /* cyan on blue (directory selected) */

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
 * Streams entries from FatFS on each call — no bulk buffer.
 * Supports directory navigation: shows "[..]" when in a subdirectory and
 * renders directories as "[name]" in cyan.
 *
 * @param num_cols        Number of file grid columns (pre-computed by caller).
 * @param total_grid_rows Total grid rows = ceil(entry_count / num_cols).
 */
static void fp_draw_screen(uint16_t file_count, uint16_t cursor,
                           uint8_t scroll_top_row,
                           int num_cols, int total_grid_rows) {
    char buf[160];
    int n;

    fp_write_str("\x1b[?25l");  /* hide cursor */

    bool not_root = fp_not_at_root();

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
        thumb_size = (list_height * list_height) / total_grid_rows;  /* floor: proportional */
        if (thumb_size < 1) thumb_size = 1;
        if (thumb_size > list_height - 1) thumb_size = list_height - 1; /* always ≥1 track cell */
        int max_scroll = total_grid_rows - list_height;
        if (max_scroll > 0)
            thumb_start = (int)scroll_top_row * (list_height - thumb_size) / max_scroll;
    }

    /* ── Row 1: title bar — show path when in a subdirectory ── */
    fp_goto(1, 1);
    fp_write_str(FP_ATTR_TITLE);
    if (not_root) {
        n = snprintf(buf, sizeof(buf), " Select File \xe2\x80\x94 %s", fp_cwd);
    } else {
        n = snprintf(buf, sizeof(buf), " Select File");
    }
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

    /* ── Rows 4..list_top+list_height-1: grid — streamed from FatFS ── */
    {
        file_entry_t row_buf[FP_MAX_COLS];   /* ≤ FP_MAX_COLS * 22 B on stack */
        DIR dir;
        FILINFO fno;

        /* Absolute index of the first item visible on screen */
        int first_item = (int)scroll_top_row * num_cols;
        int global_idx = first_item; /* tracks next item to fill into row_buf */

        /* FatFS skip count: synthetic ".." counts as item 0, so subtract 1
         * for FatFS when we're inside a subdir and the ".." is before first_item. */
        int fatfs_skip = not_root ? (first_item > 0 ? first_item - 1 : 0) : first_item;
        bool dir_ok = (f_opendir(&dir, fp_cwd) == FR_OK);
        if (dir_ok) {
            int fatfs_skipped = 0;
            while (fatfs_skipped < fatfs_skip) {
                if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
                    f_closedir(&dir);
                    dir_ok = false;
                    break;
                }
                if (fp_entry_visible(&fno)) fatfs_skipped++;
            }
        }

        for (int vis = 0; vis < list_height; vis++) {
            int grid_row = (int)scroll_top_row + vis;
            int row      = list_top + vis;

            /* Elevator: each state is a fully-attributed string.
             * Thumb = reverse-video space (solid), track = dim dot (subtle). */
            const char* elev;
            if (!need_scroll) {
                elev = " ";
            } else if (vis >= thumb_start && vis < thumb_start + thumb_size) {
                elev = "\x1b[0;7m \x1b[0m";   /* reset, reverse, space, reset */
            } else {
                elev = "\x1b[0;2m\xc2\xb7\x1b[0m"; /* reset, dim, middle dot ·, reset */
            }

            fp_goto(row, left_col);
            fp_write_str(FP_ATTR_BORDER "|");

            /* Fill row_buf with up to num_cols entries */
            uint8_t row_count = 0;
            while (row_count < (uint8_t)num_cols && grid_row < total_grid_rows
                   && global_idx < (int)file_count) {
                if (not_root && global_idx == 0) {
                    /* Synthetic ".." entry */
                    row_buf[row_count].name[0] = '.';
                    row_buf[row_count].name[1] = '.';
                    row_buf[row_count].name[2] = '\0';
                    row_buf[row_count].size_str[0] = '\0';
                    row_buf[row_count].is_dir = true;
                    row_count++;
                    global_idx++;
                } else if (dir_ok) {
                    /* Read next matching FatFS entry */
                    bool got = false;
                    while (!got) {
                        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) {
                            f_closedir(&dir);
                            dir_ok = false;
                            break;
                        }
                        if (!fp_entry_visible(&fno)) continue;
                        strncpy(row_buf[row_count].name, fno.fname, 12);
                        row_buf[row_count].name[12] = '\0';
                        row_buf[row_count].is_dir = (fno.fattrib & AM_DIR) != 0;
                        if (row_buf[row_count].is_dir) {
                            row_buf[row_count].size_str[0] = '\0';
                        } else {
                            fp_format_size((uint32_t)fno.fsize,
                                           row_buf[row_count].size_str,
                                           sizeof(row_buf[row_count].size_str));
                        }
                        row_count++;
                        global_idx++;
                        got = true;
                    }
                    if (!got) break;
                } else {
                    break;
                }
            }

            int chars_written = 0;
            for (int gc = 0; gc < (int)row_count; gc++) {
                uint16_t item_idx = (uint16_t)(grid_row * num_cols + gc);
                bool is_selected = (item_idx + 1 == cursor);

                if (row_buf[gc].is_dir) {
                    /* Directory — render as " [name]" in cyan */
                    fp_write_str(is_selected ? FP_ATTR_DIR_SEL : FP_ATTR_DIR);
                    fp_write_str(" [");
                    int name_len = (int)strlen(row_buf[gc].name);
                    if (name_len > FP_COL_NAME_WIDTH - 2) name_len = FP_COL_NAME_WIDTH - 2;
                    fp_write_buf(row_buf[gc].name, name_len);
                    fp_write_str("]");
                    int cell_used = 1 + 1 + name_len + 1; /* ' ' + '[' + name + ']' */
                    fp_write_str(FP_ATTR_NORMAL);
                    for (; cell_used < col_width; cell_used++) fp_write_str(" ");
                    chars_written += col_width;
                } else {
                    /* File — name + right-aligned size */
                    fp_write_str(is_selected ? FP_ATTR_SELECTED : FP_ATTR_NORMAL);
                    int name_len = (int)strlen(row_buf[gc].name);
                    if (name_len > FP_COL_NAME_WIDTH) name_len = FP_COL_NAME_WIDTH;
                    fp_write_str(" ");
                    fp_write_buf(row_buf[gc].name, name_len);
                    int cell_used = 1 + name_len;
                    int size_len = (int)strlen(row_buf[gc].size_str);
                    int gap = col_width - cell_used - size_len - 1;
                    if (gap >= 0) {
                        for (int p = 0; p < gap; p++) fp_write_str(" ");
                        fp_write_str(is_selected ? FP_ATTR_SIZE_SEL : FP_ATTR_SIZE);
                        fp_write_buf(row_buf[gc].size_str, size_len);
                        fp_write_str(is_selected ? FP_ATTR_SELECTED : FP_ATTR_NORMAL);
                        fp_write_str(" ");
                        cell_used += gap + size_len + 1;
                    }
                    fp_write_str(FP_ATTR_NORMAL);
                    for (; cell_used < col_width; cell_used++) fp_write_str(" ");
                    chars_written += col_width;
                }
            }

            /* Pad any remainder (empty cells or integer-division slack) */
            fp_write_str(FP_ATTR_NORMAL);
            for (; chars_written < content_width; chars_written++) fp_write_str(" ");

            /* Elevator column + right border */
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
    /* Initialise path and extension filter, then count visible entries */
    fp_cwd[0] = '/'; fp_cwd[1] = '\0';
    fp_ext_filter = ext;
    uint16_t file_count = fp_count_entries();

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
    uint16_t cursor        = (file_count > 0) ? 1 : 0;
    uint8_t scroll_top_row = 0;
    int     list_height    = (int)fp_rows - 5;
    if (list_height < 2) list_height = 2;

    bool selected = false;
    bool running  = true;

    /* Initial draw */
    fp_draw_screen(file_count, cursor, scroll_top_row, num_cols, total_grid_rows);

    while (running) {
        uint16_t prev_cursor = cursor;
        uint8_t  prev_scroll = scroll_top_row;
        int key = fp_read_key_fn();

        switch (key) {
        case VT100_KEY_UP:
            if (cursor > 0) {
                if (cursor <= (uint16_t)num_cols) {
                    /* On first grid row — go to new-file row */
                    cursor = 0;
                    scroll_top_row = 0;
                } else {
                    cursor -= (uint16_t)num_cols;
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
                uint16_t nc = cursor + (uint16_t)num_cols;
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
                    cursor = (item < (int)file_count) ? (uint16_t)(item + 1) : file_count;
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
                    cursor = (item < (int)file_count) ? (uint16_t)(item + 1) : file_count;
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
                /* "New file..." — prompt for name, build path, exit on confirm */
                char name_only[14] = {0};
                if (fp_popup_filename(name_only, sizeof(name_only))) {
                    if (fp_not_at_root()) {
                        snprintf(file_buf, buf_size, "%s/%s", fp_cwd, name_only);
                    } else {
                        snprintf(file_buf, buf_size, "/%s", name_only);
                    }
                    selected = true;
                    running = false;
                }
            } else {
                /* Pick existing entry — may be a directory or a file */
                fp_entry_info_t info;
                if (fp_read_entry_info(cursor - 1, &info)) {
                    if (info.is_dir) {
                        if (info.name[0] == '.' && info.name[1] == '.' && info.name[2] == '\0') {
                            fp_ascend();
                        } else {
                            fp_descend(info.name);
                        }
                        /* Recount and reset navigation for new directory */
                        file_count = fp_count_entries();
                        total_grid_rows = (file_count > 0)
                            ? ((int)file_count + num_cols - 1) / num_cols : 0;
                        cursor = (file_count > 0) ? 1 : 0;
                        scroll_top_row = 0;
                        fp_draw_screen(file_count, cursor, scroll_top_row, num_cols, total_grid_rows);
                    } else {
                        /* Always return absolute path */
                        if (fp_not_at_root()) {
                            snprintf(file_buf, buf_size, "%s/%s", fp_cwd, info.name);
                        } else {
                            snprintf(file_buf, buf_size, "/%s", info.name);
                        }
                        selected = true;
                        running = false;
                    }
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

        if (running && (cursor != prev_cursor || scroll_top_row != prev_scroll)) {
            fp_draw_screen(file_count, cursor, scroll_top_row, num_cols, total_grid_rows);
        }
    }

    /* Cleanup standalone mode */
    if (standalone) {
        ui_app_close(&fp_standalone_app);
    }

    return selected;
}
