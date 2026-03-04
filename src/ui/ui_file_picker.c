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

/* ── Directory scanning ─────────────────────────────────────────────── */

static uint8_t fp_scan_files(const char* ext, file_entry_t* entries, uint8_t max_entries) {
    DIR dir;
    FILINFO fno;
    uint8_t count = 0;

    if (f_opendir(&dir, "") != FR_OK) return 0;

    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & AM_DIR) continue;
        if (fno.fattrib & (AM_HID | AM_SYS)) continue;

        /* Extension filter */
        if (ext && ext[0]) {
            const char* dot = strrchr(fno.fname, '.');
            if (!dot) continue;
            dot++;
            bool match = true;
            for (int i = 0; ext[i] && dot[i]; i++) {
                char a = ext[i], b = dot[i];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
            }
            if (!match) continue;
        }

        if (count < max_entries) {
            strncpy(entries[count].name, fno.fname, 12);
            entries[count].name[12] = 0;
            fp_format_size((uint32_t)fno.fsize, entries[count].size_str,
                           sizeof(entries[count].size_str));
            count++;
        }
    }
    f_closedir(&dir);
    return count;
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

static void fp_goto(int row, int col) {
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    fp_write_buf(buf, n);
}

/**
 * Draw the fullscreen file picker.
 *
 * Layout:
 *   Row 1:       title bar (blue)
 *   Row 2:       top border +----+
 *   Row 3..N-2:  file list area (scrollable)
 *   Row N-1:     bottom border +----+
 *   Row N:       status bar
 *
 * Each file row: | FILENAME.EXT    1.2K |
 * First entry is always "[New file...]" in green
 */
static void fp_draw_screen(file_entry_t* entries, uint8_t file_count,
                           uint8_t cursor, uint8_t scroll_top) {
    char buf[160];
    int n;

    fp_write_str("\x1b[?25l");  /* hide cursor */

    /* Total items: files + 1 for "New file..." */
    uint8_t total_items = file_count + 1;  /* index 0 = new file */

    /* Layout geometry */
    int list_top = 3;                          /* first file row */
    int list_height = fp_rows - 4;             /* rows available for file list */
    if (list_height < 3) list_height = 3;

    /* Content column: leave margins */
    int left_col = 2;
    int inner_width = fp_cols - 2;  /* inside borders */
    if (inner_width < 20) inner_width = 20;

    /* Row 1: title bar */
    fp_goto(1, 1);
    fp_write_str(FP_ATTR_TITLE);
    n = snprintf(buf, sizeof(buf), " Select File");
    fp_write_buf(buf, n);
    /* Pad rest of title row */
    for (int i = n - 1; i < fp_cols; i++) fp_write_str(" ");

    /* Row 2: top border */
    fp_goto(2, left_col);
    fp_write_str(FP_ATTR_BORDER);
    fp_write_str("+");
    for (int i = 0; i < inner_width - 2; i++) fp_write_str("-");
    fp_write_str("+");
    fp_write_str(FP_ATTR_NORMAL "\x1b[K");

    /* File list rows */
    for (int vis = 0; vis < list_height; vis++) {
        int item_idx = scroll_top + vis;
        int row = list_top + vis;

        fp_goto(row, left_col);
        fp_write_str(FP_ATTR_BORDER);
        fp_write_str("|");

        if (item_idx < total_items) {
            bool is_selected = (item_idx == cursor);
            bool is_new_file = (item_idx == 0);

            /* Background for this row */
            if (is_selected) {
                fp_write_str(is_new_file ? FP_ATTR_NEW_SEL : FP_ATTR_SELECTED);
            } else {
                fp_write_str(is_new_file ? FP_ATTR_NEW_FILE : FP_ATTR_NORMAL);
            }

            if (is_new_file) {
                /* "New file..." entry */
                const char* label = " [Enter filename...]";
                int label_len = (int)strlen(label);
                fp_write_str(label);
                /* Pad to fill row */
                for (int p = label_len; p < inner_width - 2; p++) fp_write_str(" ");
            } else {
                /* File entry: " NAME         SIZE " */
                uint8_t fi = (uint8_t)(item_idx - 1);  /* index into entries[] */
                int name_len = (int)strlen(entries[fi].name);
                int size_len = (int)strlen(entries[fi].size_str);
                int name_col_width = inner_width - 2 - size_len - 2;
                if (name_col_width < 13) name_col_width = 13;

                fp_write_str(" ");
                fp_write_str(entries[fi].name);
                /* Pad between name and size */
                for (int p = name_len; p < name_col_width; p++) fp_write_str(" ");

                /* Size in yellow (or yellow-on-blue if selected) */
                if (is_selected) {
                    fp_write_str(FP_ATTR_SIZE_SEL);
                } else {
                    fp_write_str(FP_ATTR_SIZE);
                }
                fp_write_str(entries[fi].size_str);

                /* Restore row style for trailing space */
                if (is_selected) {
                    fp_write_str(FP_ATTR_SELECTED);
                } else {
                    fp_write_str(FP_ATTR_NORMAL);
                }
                fp_write_str(" ");
            }
        } else {
            /* Empty row below last file */
            fp_write_str(FP_ATTR_NORMAL);
            for (int p = 0; p < inner_width - 2; p++) fp_write_str(" ");
        }

        /* Right border */
        fp_write_str(FP_ATTR_BORDER);
        fp_write_str("|");
        fp_write_str(FP_ATTR_NORMAL "\x1b[K");
    }

    /* Bottom border row */
    fp_goto(list_top + list_height, left_col);
    fp_write_str(FP_ATTR_BORDER);
    fp_write_str("+");
    for (int i = 0; i < inner_width - 2; i++) fp_write_str("-");
    fp_write_str("+");
    fp_write_str(FP_ATTR_NORMAL "\x1b[K");

    /* Status bar — last row */
    {
        int shown = file_count;
        const char* scroll_hint = "";
        if (total_items > (uint8_t)list_height) {
            scroll_hint = "  PgUp/PgDn=Scroll";
        }
        n = snprintf(buf, sizeof(buf),
                     " %d file%s  |  Up/Down=Navigate  Enter=Select  Esc=Cancel%s",
                     shown, shown == 1 ? "" : "s", scroll_hint);

        fp_goto(fp_rows, 1);
        fp_write_str(FP_ATTR_STATUS);
        fp_write_buf(buf, n);
        /* Pad rest of status row */
        for (int i = n; i < fp_cols; i++) fp_write_str(" ");
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
    /* Scan storage for files */
    file_entry_t entries[UI_FILE_PICKER_MAX];
    uint8_t file_count = fp_scan_files(ext, entries, UI_FILE_PICKER_MAX);

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

    /* Total items: "New file" + actual files */
    uint8_t total_items = file_count + 1;

    /* Navigation state */
    uint8_t cursor = (file_count > 0) ? 1 : 0;  /* start on first real file */
    uint8_t scroll_top = 0;
    int list_height = fp_rows - 4;
    if (list_height < 3) list_height = 3;

    bool selected = false;
    bool running = true;

    /* Initial draw */
    fp_draw_screen(entries, file_count, cursor, scroll_top);

    while (running) {
        int key = fp_read_key_fn();

        switch (key) {
        case VT100_KEY_UP:
            if (cursor > 0) {
                cursor--;
                if (cursor < scroll_top) scroll_top = cursor;
            }
            break;

        case VT100_KEY_DOWN:
            if (cursor < total_items - 1) {
                cursor++;
                if (cursor >= scroll_top + (uint8_t)list_height) {
                    scroll_top = cursor - (uint8_t)list_height + 1;
                }
            }
            break;

        case VT100_KEY_PAGEUP:
            if (cursor > (uint8_t)list_height) {
                cursor -= (uint8_t)list_height;
            } else {
                cursor = 0;
            }
            if (cursor < scroll_top) scroll_top = cursor;
            break;

        case VT100_KEY_PAGEDOWN:
            cursor += (uint8_t)list_height;
            if (cursor >= total_items) cursor = total_items - 1;
            if (cursor >= scroll_top + (uint8_t)list_height) {
                scroll_top = cursor - (uint8_t)list_height + 1;
            }
            break;

        case VT100_KEY_HOME:
            cursor = 0;
            scroll_top = 0;
            break;

        case VT100_KEY_END:
            cursor = total_items - 1;
            if (cursor >= (uint8_t)list_height) {
                scroll_top = cursor - (uint8_t)list_height + 1;
            }
            break;

        case VT100_KEY_ENTER:
        case '\n':
            if (cursor == 0) {
                /* "New file..." — show popup */
                selected = fp_popup_filename(file_buf, buf_size);
            } else {
                /* Pick existing file */
                uint8_t fi = cursor - 1;
                strncpy(file_buf, entries[fi].name, buf_size - 1);
                file_buf[buf_size - 1] = 0;
                selected = true;
            }
            running = false;
            break;

        case VT100_KEY_ESC:
        case VT100_KEY_CTRL_Q:
            running = false;
            break;

        default:
            break;
        }

        if (running) {
            fp_draw_screen(entries, file_count, cursor, scroll_top);
        }
    }

    /* Cleanup standalone mode */
    if (standalone) {
        ui_app_close(&fp_standalone_app);
    }

    return selected;
}
