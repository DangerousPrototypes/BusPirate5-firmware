/**
 * @file ui_cmd_menu.c
 * @brief Interactive VT100 wizard for command actions, device selection, files.
 * @details Full-featured interactive wizard that walks users through all
 *          command parameters via VT100 dropdown menus. Supports action
 *          selection, device picking, file browsing, and confirmation dialogs.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "ui/ui_cmd_menu.h"
#include "ui/ui_popup.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "fatfs/ff.h"
#include "lib/vt100_menu/vt100_menu.h"
#include "lib/vt100_keys/vt100_keys.h"
#include "lib/bp_args/bp_cmd.h"

/* ── Session state (persists across picks within one wizard) ────────── */
static struct {
    vt100_key_state_t ks;
    uint8_t rows;
    uint8_t cols;
    const char* title;
    uint8_t status_row;   /* next row for status line (starts at 3) */
    bool is_open;
} wiz;

/* ── I/O callbacks ──────────────────────────────────────────────────── */
static int wiz_read_blocking(char* c) {
    rx_fifo_get_blocking(c);
    return 1;
}

static int wiz_read_try(char* c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

static int wiz_read_key_cb(void) {
    return vt100_key_read(&wiz.ks);
}

static int wiz_write_cb(int fd, const void* buf, int count) {
    (void)fd;
    tx_fifo_write((const char*)buf, (uint32_t)count);
    return count;
}

static void wiz_write_str(const char* s) {
    tx_fifo_write(s, strlen(s));
}

/* ── Screen drawing helpers ─────────────────────────────────────────── */

/**
 * Draw the full wizard background: title bar, status lines, footer.
 * Called on initial open and as repaint callback for the menu framework.
 */
static void wiz_draw_background(void) {
    char buf[120];
    int n;

    wiz_write_str("\x1b[?25l");  /* hide cursor */

    /* Row 1: menu bar (drawn by vt100_menu framework, leave blank here) */
    wiz_write_str("\x1b[1;1H\x1b[K");

    /* Row 2: title bar */
    wiz_write_str("\x1b[2;1H\x1b[0m\x1b[K\x1b[1;37;44m");
    n = snprintf(buf, sizeof(buf), "  %s  ", wiz.title ? wiz.title : "Command");
    wiz_write_str(buf);
    wiz_write_str("\x1b[0m");

    /* Rows 3..status_row-1: previously drawn status lines (don't erase) */

    /* Clear rows from current status_row down to leave room for dropdown */
    for (int r = wiz.status_row; r < wiz.rows && r <= 22; r++) {
        n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[K", r);
        wiz_write_str(buf);
    }

    /* Footer / status bar */
    n = snprintf(buf, sizeof(buf),
                 "\x1b[%d;1H\x1b[0;30;47m " UI_HINT_ARROWS_SELECT_CANCEL " \x1b[0m\x1b[K",
                 wiz.rows);
    wiz_write_str(buf);

    wiz_write_str("\x1b[?25h");  /* show cursor */
}

static void wiz_repaint_cb(void) {
    wiz_draw_background();
}

/* ── Internal: run a dropdown within the current session ────────────── */
static int wiz_run_menu(const char* menu_label,
                        vt100_menu_item_t* items,
                        uint8_t count) {
    vt100_menu_def_t menu = { menu_label, items, count };

    vt100_menu_state_t ms;
    vt100_menu_init(&ms, &menu, 1, 1,
                    wiz.cols, wiz.rows,
                    wiz_read_key_cb, wiz_write_cb);
    ms.repaint = wiz_repaint_cb;

    tx_fifo_wait_drain();
    wiz_draw_background();
    vt100_menu_draw_bar(&ms);

    int result = vt100_menu_run(&ms);

    /* Erase dropdown remnants */
    wiz_draw_background();

    return result;
}

/* ════════════════════════════════════════════════════════════════════════
 * Public API: Session lifecycle
 * ════════════════════════════════════════════════════════════════════════ */

void ui_cmd_menu_open(const char* title) {
    toolbar_draw_prepare();
    printf("\x1b[?1049h\x1b[r\x1b[2J\x1b[H");
    { char d; while (rx_fifo_try_get(&d)) {} }

    wiz.rows       = (uint8_t)system_config.terminal_ansi_rows;
    wiz.cols       = (uint8_t)system_config.terminal_ansi_columns;
    wiz.title      = title;
    wiz.status_row = 4;  /* first status line at row 4 (row 2=title, 3=blank) */
    wiz.is_open    = true;

    vt100_key_init(&wiz.ks, wiz_read_blocking, wiz_read_try);
}

void ui_cmd_menu_close(void) {
    if (!wiz.is_open) return;
    { char d; while (rx_fifo_try_get(&d)) {} }
    printf("\x1b[?1049l");
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();
    wiz.is_open = false;
}

void ui_cmd_menu_status(const char* label, const char* value) {
    if (!wiz.is_open) return;
    char buf[120];
    int n = snprintf(buf, sizeof(buf),
                     "\x1b[%d;3H\x1b[0;36m%s:\x1b[0;1;37m %s\x1b[0m\x1b[K",
                     wiz.status_row, label, value);
    wiz_write_str(buf);
    wiz.status_row++;
    if (wiz.status_row > wiz.rows - 4) {
        wiz.status_row = wiz.rows - 4;  /* clamp to avoid overflowing into footer */
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * Pick action from command definition
 * ════════════════════════════════════════════════════════════════════════ */
bool ui_cmd_menu_pick_action(const struct bp_command_def* def, uint32_t* action) {
    if (!def || !def->actions || def->action_count == 0 || !action) {
        return false;
    }

    uint8_t n = def->action_count;
    if (n > CMD_MENU_MAX_ITEMS) n = CMD_MENU_MAX_ITEMS;

    vt100_menu_item_t items[CMD_MENU_MAX_ITEMS + 1];
    for (uint8_t i = 0; i < n; i++) {
        items[i].label     = def->actions[i].verb;
        items[i].shortcut  = NULL;
        items[i].action_id = (int)(def->actions[i].action + 1);  /* +1 to avoid 0 */
        items[i].flags     = 0;
    }
    items[n] = (vt100_menu_item_t){ NULL, NULL, 0, 0 };

    int result = wiz_run_menu("Action", items, n);

    if (result > 0) {
        *action = (uint32_t)(result - 1);
        return true;
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════════════
 * Pick device from a name array
 * ════════════════════════════════════════════════════════════════════════ */
bool ui_cmd_menu_pick_device(const char* const* names,
                             uint8_t count,
                             uint8_t* selected) {
    if (!names || count == 0 || !selected) {
        return false;
    }

    uint8_t n = count;
    if (n > CMD_MENU_MAX_ITEMS) n = CMD_MENU_MAX_ITEMS;

    vt100_menu_item_t items[CMD_MENU_MAX_ITEMS + 1];
    for (uint8_t i = 0; i < n; i++) {
        items[i].label     = names[i];
        items[i].shortcut  = NULL;
        items[i].action_id = (int)(i + 1);
        items[i].flags     = 0;
    }
    items[n] = (vt100_menu_item_t){ NULL, NULL, 0, 0 };

    int result = wiz_run_menu("Device", items, n);

    if (result > 0) {
        *selected = (uint8_t)(result - 1);
        return true;
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════════════
 * Pick file from internal storage
 * ════════════════════════════════════════════════════════════════════════ */
bool ui_cmd_menu_pick_file(const char* ext, char* file_buf, uint8_t buf_size) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;

    /* Scan directory for matching files.
     * Store names in a stack buffer — 8.3 filenames are max 13 chars. */
    #define FILE_PICK_MAX 24
    char file_names[FILE_PICK_MAX][13];
    uint8_t file_count = 0;

    fr = f_opendir(&dir, "");
    if (fr != FR_OK) {
        /* Storage not mounted or error — show message and fail */
        if (wiz.is_open) {
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "\x1b[%d;3H\x1b[0;31mNo storage available\x1b[0m",
                     wiz.status_row);
            wiz_write_str(buf);
        }
        return false;
    }

    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & AM_DIR) continue;           /* skip directories */
        if (fno.fattrib & (AM_HID | AM_SYS)) continue; /* skip hidden/system */

        /* Extension filter */
        if (ext && ext[0]) {
            const char* dot = strrchr(fno.fname, '.');
            if (!dot) continue;
            dot++;
            /* Case-insensitive compare */
            bool match = true;
            for (int i = 0; ext[i] && dot[i]; i++) {
                char a = ext[i], b = dot[i];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
            }
            if (!match) continue;
        }

        if (file_count < FILE_PICK_MAX) {
            strncpy(file_names[file_count], fno.fname, 12);
            file_names[file_count][12] = 0;
            strlwr(file_names[file_count]);
            file_count++;
        }
    }
    f_closedir(&dir);

    if (file_count == 0) {
        if (wiz.is_open) {
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "\x1b[%d;3H\x1b[0;33mNo %s files found on storage\x1b[0m",
                     wiz.status_row, ext ? ext : "");
            wiz_write_str(buf);
        }
        return false;
    }

    /* Build menu items — point labels into our stack buffer */
    uint8_t n = file_count;
    if (n > CMD_MENU_MAX_ITEMS) n = CMD_MENU_MAX_ITEMS;

    vt100_menu_item_t items[CMD_MENU_MAX_ITEMS + 1];
    for (uint8_t i = 0; i < n; i++) {
        items[i].label     = file_names[i];
        items[i].shortcut  = NULL;
        items[i].action_id = (int)(i + 1);
        items[i].flags     = 0;
    }
    items[n] = (vt100_menu_item_t){ NULL, NULL, 0, 0 };

    int result = wiz_run_menu("File", items, n);

    if (result > 0) {
        uint8_t idx = (uint8_t)(result - 1);
        strncpy(file_buf, file_names[idx], buf_size - 1);
        file_buf[buf_size - 1] = 0;
        return true;
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════════════
 * Yes/No confirmation dialog
 * ════════════════════════════════════════════════════════════════════════ */
bool ui_cmd_menu_confirm(const char* message) {
    static const vt100_menu_item_t confirm_items[] = {
        { "Yes - continue",  "Y",  1, 0 },
        { "No - cancel",     "N",  2, 0 },
        { NULL, NULL, 0, 0 },
    };

    /* Show the question as a status line */
    if (wiz.is_open && message) {
        char buf[120];
        snprintf(buf, sizeof(buf),
                 "\x1b[%d;3H\x1b[0;33m%s\x1b[0m\x1b[K",
                 wiz.status_row, message);
        wiz_write_str(buf);
        wiz.status_row++;
    }

    /* Use non-const copy since wiz_run_menu takes non-const items */
    vt100_menu_item_t items[3];
    memcpy(items, confirm_items, sizeof(items));

    int result = wiz_run_menu("Confirm", items, 2);

    return (result == 1);  /* 1 = Yes */
}

/* ════════════════════════════════════════════════════════════════════════
 * Numeric input prompt (hex or decimal)
 * ════════════════════════════════════════════════════════════════════════ */
bool ui_cmd_menu_pick_number(const char* prompt, uint32_t def_val, uint32_t* result) {
    if (!wiz.is_open || !result) return false;

    char buf[120];
    char input[16] = {0};
    uint8_t pos = 0;

    /* Draw prompt with default value */
    snprintf(buf, sizeof(buf),
             "\x1b[%d;3H\x1b[0;33m%s [0x%X]: \x1b[0;1;37m",
             wiz.status_row, prompt ? prompt : "Value", (unsigned)def_val);
    wiz_write_str(buf);
    wiz_write_str("\x1b[?25h");  /* show cursor */

    for (;;) {
        char c;
        rx_fifo_get_blocking(&c);

        if (c == '\r' || c == '\n') {
            /* Accept: parse or use default */
            if (pos == 0) {
                *result = def_val;
            } else {
                input[pos] = 0;
                /* Parse hex (0x prefix) or decimal */
                if (pos > 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
                    *result = (uint32_t)strtoul(&input[2], NULL, 16);
                } else {
                    *result = (uint32_t)strtoul(input, NULL, 0);
                }
            }
            wiz_write_str("\x1b[0m\x1b[K");
            wiz.status_row++;
            return true;
        }

        if (c == 0x1b) {
            /* Escape — cancel */
            wiz_write_str("\x1b[0m\x1b[K");
            return false;
        }

        if (c == 0x7f || c == '\b') {
            /* Backspace */
            if (pos > 0) {
                pos--;
                wiz_write_str("\b \b");
            }
            continue;
        }

        /* Accept hex digits, 'x', 'X' */
        if (pos < sizeof(input) - 1) {
            bool valid = (c >= '0' && c <= '9') ||
                         (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F') ||
                         (c == 'x' || c == 'X');
            if (valid) {
                input[pos++] = c;
                char echo[2] = {c, 0};
                wiz_write_str(echo);
            }
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * Pick from a generic string list
 * ════════════════════════════════════════════════════════════════════════ */
bool ui_cmd_menu_pick_list(const char* menu_label,
                           const char* const* items_str,
                           uint8_t count,
                           uint8_t* selected) {
    if (!items_str || count == 0 || !selected) return false;

    uint8_t n = count;
    if (n > CMD_MENU_MAX_ITEMS) n = CMD_MENU_MAX_ITEMS;

    vt100_menu_item_t items[CMD_MENU_MAX_ITEMS + 1];
    for (uint8_t i = 0; i < n; i++) {
        items[i].label     = items_str[i];
        items[i].shortcut  = NULL;
        items[i].action_id = (int)(i + 1);
        items[i].flags     = 0;
    }
    items[n] = (vt100_menu_item_t){ NULL, NULL, 0, 0 };

    int result = wiz_run_menu(menu_label ? menu_label : "Select", items, n);

    if (result > 0) {
        *selected = (uint8_t)(result - 1);
        return true;
    }
    return false;
}
