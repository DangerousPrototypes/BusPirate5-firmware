/**
 * @file eeprom_i2c_gui.c
 * @brief Fullscreen interactive GUI for I2C EEPROM operations.
 *
 * Menu-bar-driven fullscreen app following the menu_demo.c pattern.
 * Four top-level menus: Action, Device, File, Options.
 * The user configures an operation via the menus, then executes it.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "fatfs/ff.h"
#include "lib/vt100_menu/vt100_menu.h"
#include "lib/vt100_keys/vt100_keys.h"
#include "ui/ui_file_picker.h"
#include "ui/ui_hex.h"
#include "ui/ui_progress_indicator.h"
#include "binmode/fala.h"
#include "mode/hwi2c.h"
#include "eeprom_base.h"
#include "eeprom_i2c_gui.h"

/* ── Action IDs ─────────────────────────────────────────────────────── */
/* Positive IDs returned by vt100_menu_run(). Grouped by menu. */
enum {
    /* Action menu (1-9) */
    ACT_DUMP    = 1,
    ACT_ERASE   = 2,
    ACT_WRITE   = 3,
    ACT_READ    = 4,
    ACT_VERIFY  = 5,
    ACT_TEST    = 6,

    /* Device menu (10 + device index) */
    ACT_DEV_BASE = 10,
    /* ACT_DEV_BASE + 0 = first device, +1 = second, etc. */

    /* File menu (100+) */
    ACT_FILE_NEW   = 100,

    /* Options menu (200+) */
    ACT_OPT_VERIFY  = 200,
    ACT_OPT_ADDR    = 201,
    ACT_OPT_EXECUTE = 210,
    ACT_OPT_QUIT    = 211,
};

/* ── App state ──────────────────────────────────────────────────────── */

/* Action names for display (indexed by action enum from eeprom_i2c.c) */
static const char* const action_names[] = {
    "dump", "erase", "write", "read", "verify", "test",
};
#define ACTION_COUNT 6

/* Focus field indices for Tab-cycling config bar */
enum {
    FOCUS_ACTION  = 0,
    FOCUS_DEVICE  = 1,
    FOCUS_FILE    = 2,
    FOCUS_ADDR    = 3,
    FOCUS_VERIFY  = 4,
    FOCUS_EXECUTE = 5,
    FOCUS_COUNT   = 6,
};

typedef struct {
    /* Terminal */
    uint8_t rows;
    uint8_t cols;

    /* App lifecycle */
    bool running;
    bool menu_pending;
    bool executed;          /* true after an operation has been run */

    /* Key decoder */
    vt100_key_state_t keys;

    /* Configuration — mirrors eeprom_info fields */
    int      action;        /* -1 = not set, else 0..5 */
    int      device_idx;    /* -1 = not set, else index into devices[] */
    char     file_name[13]; /* empty = not set */
    uint8_t  i2c_addr;      /* 7-bit, default 0x50 */
    bool     verify_flag;
    int      focused_field;  /* FOCUS_* index for Tab-cycling config bar */

    /* Device table reference (not owned) */
    const struct eeprom_device_t* devices;
    uint8_t device_count;

    /* Output area */
    const char* status_msg;  /* one-line status at bottom */
    const char* result_msg;  /* result after execution */
} eeprom_gui_t;

static eeprom_gui_t gui;

/* ── I/O helpers ────────────────────────────────────────────────────── */

static void gui_write_str(const char* s) {
    tx_fifo_write(s, strlen(s));
}

static void gui_write_buf(const void* buf, int len) {
    tx_fifo_write((const char*)buf, (uint32_t)len);
}

static int gui_read_blocking(char* c) {
    rx_fifo_get_blocking(c);
    return 1;
}

static int gui_read_try(char* c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

static int gui_menu_read_key(void) {
    return vt100_key_read(&gui.keys);
}

static int gui_menu_write(int fd, const void* buf, int count) {
    (void)fd;
    tx_fifo_write((const char*)buf, (uint32_t)count);
    return count;
}

/* ── Size formatting helper ─────────────────────────────────────────── */

static void format_size(uint32_t bytes, char* buf, uint8_t buf_size) {
    if (bytes < 1024) {
        snprintf(buf, buf_size, "%luB", (unsigned long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, buf_size, "%luK", (unsigned long)(bytes / 1024));
    } else {
        snprintf(buf, buf_size, "%luM", (unsigned long)(bytes / (1024 * 1024)));
    }
}

/* ── Helper: does current action need a file? ───────────────────────── */

static bool action_needs_file(int action) {
    return (action == 2 || action == 3 || action == 4);
    /* EEPROM_WRITE=2, EEPROM_READ=3, EEPROM_VERIFY=4 (0-based: write=2, read=3, verify=4) */
}

/* ── Helper: is config complete enough to execute? ──────────────────── */

static bool config_ready(void) {
    if (gui.action < 0 || gui.device_idx < 0) return false;
    if (action_needs_file(gui.action) && gui.file_name[0] == 0) return false;
    return true;
}

/* ── Focus navigation helpers ───────────────────────────────────────── */

static int focus_next(int current) {
    for (int i = 0; i < FOCUS_COUNT; i++) {
        current = (current + 1) % FOCUS_COUNT;
        if (current == FOCUS_FILE && (gui.action < 0 || !action_needs_file(gui.action)))
            continue;
        return current;
    }
    return FOCUS_ACTION;
}

static int focus_prev(int current) {
    for (int i = 0; i < FOCUS_COUNT; i++) {
        current = (current - 1 + FOCUS_COUNT) % FOCUS_COUNT;
        if (current == FOCUS_FILE && (gui.action < 0 || !action_needs_file(gui.action)))
            continue;
        return current;
    }
    return FOCUS_ACTION;
}

/* ── Screen refresh ─────────────────────────────────────────────────── */

static void gui_refresh_screen(void) {
    char buf[200];
    int n;

    gui_write_str("\x1b[?25l");  /* hide cursor */

    /* Validate focus — skip file field if action doesn't need one */
    if (gui.focused_field == FOCUS_FILE &&
        (gui.action < 0 || !action_needs_file(gui.action))) {
        gui.focused_field = focus_next(FOCUS_FILE);
    }

    /* Row 1: menu bar — drawn by framework, leave alone */

    /* Row 2: config bar with bracket-dropdown fields */
    gui_write_str("\x1b[2;1H\x1b[0m\x1b[K");

    /* Action [write  v] */
    {
        const char* val = (gui.action >= 0 && gui.action < ACTION_COUNT)
                          ? action_names[gui.action] : "------";
        bool focused = (gui.focused_field == FOCUS_ACTION);
        gui_write_str(focused ? " \x1b[7m[" : " \x1b[36m[");
        n = snprintf(buf, sizeof(buf), "%-6s v", val);
        gui_write_buf(buf, n);
        gui_write_str(focused ? "]\x1b[0m" : "]\x1b[0m");
    }

    /* Device [24LC256 v] + size */
    {
        const char* val = (gui.device_idx >= 0 && gui.device_idx < gui.device_count)
                          ? gui.devices[gui.device_idx].name : "--------";
        bool focused = (gui.focused_field == FOCUS_DEVICE);
        gui_write_str(focused ? " \x1b[7m[" : " \x1b[36m[");
        n = snprintf(buf, sizeof(buf), "%-8s v", val);
        gui_write_buf(buf, n);
        gui_write_str(focused ? "]\x1b[0m" : "]\x1b[0m");
        if (gui.device_idx >= 0 && gui.device_idx < gui.device_count) {
            char sz[8];
            format_size(gui.devices[gui.device_idx].size_bytes, sz, sizeof(sz));
            n = snprintf(buf, sizeof(buf), " \x1b[2m%s\x1b[0m", sz);
            gui_write_buf(buf, n);
        }
    }

    /* File [DATA.BIN    ] — only when action needs a file */
    if (gui.action >= 0 && action_needs_file(gui.action)) {
        const char* val = gui.file_name[0] ? gui.file_name : "............";
        bool focused = (gui.focused_field == FOCUS_FILE);
        gui_write_str(focused ? " \x1b[7m[" : " \x1b[36m[");
        n = snprintf(buf, sizeof(buf), "%-12s", val);
        gui_write_buf(buf, n);
        gui_write_str(focused ? "]\x1b[0m" : "]\x1b[0m");
    }

    /* I2C Addr [0x50] */
    {
        bool focused = (gui.focused_field == FOCUS_ADDR);
        gui_write_str(focused ? " \x1b[7m[" : " \x1b[36m[");
        n = snprintf(buf, sizeof(buf), "0x%02X", gui.i2c_addr);
        gui_write_buf(buf, n);
        gui_write_str(focused ? "]\x1b[0m" : "]\x1b[0m");
    }

    /* Verify [xVfy] or [ Vfy] */
    {
        bool focused = (gui.focused_field == FOCUS_VERIFY);
        gui_write_str(focused ? " \x1b[7m[" : " \x1b[36m[");
        n = snprintf(buf, sizeof(buf), "%cVfy", gui.verify_flag ? 'x' : ' ');
        gui_write_buf(buf, n);
        gui_write_str(focused ? "]\x1b[0m" : "]\x1b[0m");
    }

    /* Execute button [Execute] */
    {
        bool focused = (gui.focused_field == FOCUS_EXECUTE);
        bool ready = config_ready();
        if (focused) {
            gui_write_str(ready ? " \x1b[1;37;42m[" : " \x1b[7m[");
        } else {
            gui_write_str(ready ? " \x1b[1;32m[" : " \x1b[2m[");
        }
        gui_write_str("Execute");
        gui_write_str(focused ? "]\x1b[0m" : "]\x1b[0m");
    }

    /* Row 3: separator */
    gui_write_str("\x1b[3;1H\x1b[0;2m");
    for (int i = 0; i < gui.cols; i++) gui_write_str("-");
    gui_write_str("\x1b[0m");

    /* Rows 4..(rows-1): content area — clear ALL rows first to avoid
     * repaint artifacts when switching between menus with left/right. */
    for (int r = 4; r < gui.rows; r++) {
        n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[K", r);
        gui_write_buf(buf, n);
    }

    /* Draw content into the cleared area */
    if (gui.result_msg) {
        n = snprintf(buf, sizeof(buf), "\x1b[5;3H\x1b[0;32m%s\x1b[0m", gui.result_msg);
        gui_write_buf(buf, n);
    } else if (!config_ready()) {
        /* Show hints */
        gui_write_str("\x1b[5;3H\x1b[0;2mUse Tab to move between fields, Up/Down to change values\x1b[0m");
        if (gui.action < 0) {
            gui_write_str("\x1b[6;3H\x1b[0;33m-> Select an action (Up/Down on Action field)\x1b[0m");
        } else if (gui.device_idx < 0) {
            gui_write_str("\x1b[6;3H\x1b[0;33m-> Select a device (Up/Down on Device field)\x1b[0m");
        } else if (action_needs_file(gui.action) && gui.file_name[0] == 0) {
            gui_write_str("\x1b[6;3H\x1b[0;33m-> Select a file (Enter on File field)\x1b[0m");
        }
    } else {
        /* Ready to execute */
        gui_write_str("\x1b[5;3H\x1b[0;1;32mReady! \x1b[0mTab to [Execute] and press Enter.");
    }

    /* Status bar on last row */
    {
        const char* hint = gui.status_msg ? gui.status_msg
                         : "Tab=Next  Up/Dn=Change  Enter=Select  F10=Menu  Esc=Quit";
        n = snprintf(buf, sizeof(buf),
                     "\x1b[%d;1H\x1b[0;30;47m %-*s\x1b[0m",
                     gui.rows, gui.cols - 1, hint);
        gui_write_buf(buf, n);
    }

    gui_write_str("\x1b[?25h");  /* show cursor */
}

/* Repaint callback for the menu framework */
static void gui_repaint_cb(void) {
    gui_refresh_screen();
}

/* ── Menu definitions ───────────────────────────────────────────────── */

static const vt100_menu_item_t action_items[] = {
    { "Dump",   NULL, ACT_DUMP,   0 },
    { "Erase",  NULL, ACT_ERASE,  0 },
    { "Write",  NULL, ACT_WRITE,  0 },
    { "Read",   NULL, ACT_READ,   0 },
    { "Verify", NULL, ACT_VERIFY, 0 },
    { "Test",   NULL, ACT_TEST,   0 },
};

/* Device and File menus are built dynamically — see gui_build_*_menu() */

/* Options menu — built dynamically for the checkmark on Verify */

/* ── Dynamic menu builders ──────────────────────────────────────────── */

/* Device menu items — up to 14 devices + 2 separators + sentinel */
static vt100_menu_item_t dev_items[18];
static char dev_hints[14][8];  /* size hint strings like "256B" */

/*
 * Size thresholds for separator insertion:
 *   <= 2K   = "small" (1-byte address devices: 24X01..24X16)
 *   <= 64K  = "medium" (2-byte address, standard: 24X32..24X512)
 *   > 64K   = "large" (block-select devices: 24X1025..24XM02)
 */
static vt100_menu_def_t gui_build_device_menu(void) {
    uint8_t src = gui.device_count;
    if (src > 14) src = 14;

    uint8_t idx = 0;
    uint8_t prev_category = 0;  /* 0=none, 1=small, 2=medium, 3=large */

    for (uint8_t i = 0; i < src; i++) {
        /* Determine size category */
        uint8_t category;
        if (gui.devices[i].size_bytes <= 2048) {
            category = 1;
        } else if (gui.devices[i].size_bytes <= 65536) {
            category = 2;
        } else {
            category = 3;
        }

        /* Insert separator on category boundary */
        if (prev_category != 0 && category != prev_category) {
            dev_items[idx].label     = NULL;
            dev_items[idx].shortcut  = NULL;
            dev_items[idx].action_id = 0;
            dev_items[idx].flags     = MENU_ITEM_SEPARATOR;
            idx++;
        }
        prev_category = category;

        format_size(gui.devices[i].size_bytes, dev_hints[i], sizeof(dev_hints[i]));
        dev_items[idx].label     = gui.devices[i].name;
        dev_items[idx].shortcut  = dev_hints[i];
        dev_items[idx].action_id = ACT_DEV_BASE + i;
        dev_items[idx].flags     = 0;
        idx++;
    }

    vt100_menu_def_t menu = { "Device", dev_items, idx };
    return menu;
}

/* File menu — now handled via the reusable ui_file_picker.
 * Instead of building a file menu, we launch the picker on demand.
 * The "File" menu bar entry has a single "Browse..." item. */
static vt100_menu_item_t file_menu_item[1];

static vt100_menu_def_t gui_build_file_menu(void) {
    file_menu_item[0].label     = "Browse storage...";
    file_menu_item[0].shortcut  = NULL;
    file_menu_item[0].action_id = ACT_FILE_NEW;  /* triggers the file picker */
    file_menu_item[0].flags     = 0;

    vt100_menu_def_t menu = { "File", file_menu_item, 1 };
    return menu;
}

/* Options menu items */
static vt100_menu_item_t opt_items_buf[6];

static vt100_menu_def_t gui_build_options_menu(void) {
    uint8_t idx = 0;

    opt_items_buf[idx].label     = "Verify after operation";
    opt_items_buf[idx].shortcut  = NULL;
    opt_items_buf[idx].action_id = ACT_OPT_VERIFY;
    opt_items_buf[idx].flags     = gui.verify_flag ? MENU_ITEM_CHECKED : 0;
    idx++;

    opt_items_buf[idx].label     = "I2C Address...";
    opt_items_buf[idx].shortcut  = NULL;
    opt_items_buf[idx].action_id = ACT_OPT_ADDR;
    opt_items_buf[idx].flags     = 0;
    idx++;

    /* Separator */
    opt_items_buf[idx].label     = NULL;
    opt_items_buf[idx].shortcut  = NULL;
    opt_items_buf[idx].action_id = 0;
    opt_items_buf[idx].flags     = MENU_ITEM_SEPARATOR;
    idx++;

    opt_items_buf[idx].label     = "Execute";
    opt_items_buf[idx].shortcut  = "Enter";
    opt_items_buf[idx].action_id = ACT_OPT_EXECUTE;
    opt_items_buf[idx].flags     = config_ready() ? 0 : MENU_ITEM_DISABLED;
    idx++;

    /* Separator */
    opt_items_buf[idx].label     = NULL;
    opt_items_buf[idx].shortcut  = NULL;
    opt_items_buf[idx].action_id = 0;
    opt_items_buf[idx].flags     = MENU_ITEM_SEPARATOR;
    idx++;

    opt_items_buf[idx].label     = "Quit";
    opt_items_buf[idx].shortcut  = "^Q";
    opt_items_buf[idx].action_id = ACT_OPT_QUIT;
    opt_items_buf[idx].flags     = 0;
    idx++;

    vt100_menu_def_t menu = { "Options", opt_items_buf, idx };
    return menu;
}

/* ── File picker integration ────────────────────────────────────────── */

static bool gui_pick_file(void) {
    ui_file_picker_io_t io = {
        .read_key  = gui_menu_read_key,
        .write_out = gui_menu_write,
        .repaint   = gui_repaint_cb,
        .cols      = gui.cols,
        .rows      = gui.rows,
    };
    return ui_file_pick(NULL, gui.file_name, sizeof(gui.file_name), &io);
}

/* ── Styled popup for I2C address input ─────────────────────────────── */

/**
 * Draw a centered popup and prompt for a 7-bit I2C address.
 * Uses the same visual style as the file picker's filename popup.
 */
static bool gui_input_i2c_addr(void) {
    char buf[120];
    char input[8] = {0};
    uint8_t pos = 0;
    int n;

    /* Popup dimensions */
    int popup_w = 38;
    int popup_h = 5;
    int popup_left = (gui.cols - popup_w) / 2 + 1;
    int popup_top  = (gui.rows - popup_h) / 2;
    if (popup_left < 1) popup_left = 1;
    if (popup_top < 2) popup_top = 2;

    /* Input field geometry */
    int field_row = popup_top + 3;
    int field_col = popup_left + 3;
    int field_width = 6;  /* "0x7F" max */

    /* Popup attribute macros matching the file picker palette */
    #define ADDR_POPUP_BD "\x1b[1;37;44m"
    #define ADDR_POPUP_BG "\x1b[0;37;44m"
    #define ADDR_INPUT    "\x1b[0;1;37;40m"

    gui_write_str("\x1b[?25l");  /* hide cursor during draw */

    /* Top border */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_POPUP_BD "+", popup_top, popup_left);
    gui_write_buf(buf, n);
    for (int i = 0; i < popup_w - 2; i++) gui_write_str("-");
    gui_write_str("+");

    /* Title row */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_POPUP_BD "|" ADDR_POPUP_BG, popup_top + 1, popup_left);
    gui_write_buf(buf, n);
    n = snprintf(buf, sizeof(buf), "  I2C Address (7-bit) [0x%02X]", gui.i2c_addr);
    gui_write_buf(buf, n);
    for (int i = n - 1; i < popup_w - 2; i++) gui_write_str(" ");
    gui_write_str(ADDR_POPUP_BD "|");

    /* Blank row */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_POPUP_BD "|" ADDR_POPUP_BG, popup_top + 2, popup_left);
    gui_write_buf(buf, n);
    for (int i = 0; i < popup_w - 2; i++) gui_write_str(" ");
    gui_write_str(ADDR_POPUP_BD "|");

    /* Input field row */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_POPUP_BD "|" ADDR_POPUP_BG "  " ADDR_INPUT, popup_top + 3, popup_left);
    gui_write_buf(buf, n);
    for (int i = 0; i < field_width; i++) gui_write_str(" ");
    gui_write_str(ADDR_POPUP_BG);
    for (int i = field_width + 2; i < popup_w - 2; i++) gui_write_str(" ");
    gui_write_str(ADDR_POPUP_BD "|");

    /* Hint row */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_POPUP_BD "|" ADDR_POPUP_BG "\x1b[2m", popup_top + 4, popup_left);
    gui_write_buf(buf, n);
    const char* hint = "  Enter=OK  Esc=Cancel";
    gui_write_str(hint);
    for (int i = (int)strlen(hint); i < popup_w - 2; i++) gui_write_str(" ");
    gui_write_str(ADDR_POPUP_BD "|");

    /* Bottom border */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_POPUP_BD "+", popup_top + 5, popup_left);
    gui_write_buf(buf, n);
    for (int i = 0; i < popup_w - 2; i++) gui_write_str("-");
    gui_write_str("+");

    /* Position cursor in input field */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH" ADDR_INPUT "\x1b[?25h", field_row, field_col);
    gui_write_buf(buf, n);

    #undef ADDR_POPUP_BD
    #undef ADDR_POPUP_BG
    #undef ADDR_INPUT

    /* Input loop */
    for (;;) {
        char c;
        rx_fifo_get_blocking(&c);

        if (c == '\r' || c == '\n') {
            if (pos == 0) {
                /* Keep default */
                gui_write_str("\x1b[0m");
                return true;
            }
            input[pos] = 0;
            uint32_t val;
            if (pos > 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
                val = (uint32_t)strtoul(&input[2], NULL, 16);
            } else {
                val = (uint32_t)strtoul(input, NULL, 0);
            }
            if (val > 0x7F) {
                gui.status_msg = "Invalid: address must be 0x00-0x7F";
                gui_write_str("\x1b[0m");
                return false;
            }
            gui.i2c_addr = (uint8_t)val;
            gui_write_str("\x1b[0m");
            return true;
        }

        if (c == 0x1b) {
            gui_write_str("\x1b[0m");
            return false;
        }

        if (c == 0x7f || c == '\b') {
            if (pos > 0) {
                pos--;
                n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[0;1;37;40m", field_row, field_col);
                gui_write_buf(buf, n);
                gui_write_buf(input, pos);
                gui_write_str(" ");
                for (int i = pos + 1; i < field_width; i++) gui_write_str(" ");
                n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", field_row, field_col + pos);
                gui_write_buf(buf, n);
            }
            continue;
        }

        if (pos < sizeof(input) - 1) {
            bool valid = (c >= '0' && c <= '9') ||
                         (c >= 'a' && c <= 'f') ||
                         (c >= 'A' && c <= 'F') ||
                         (c == 'x' || c == 'X');
            if (valid) {
                input[pos++] = c;
                char echo[2] = {c, 0};
                gui_write_str(echo);
            }
        }
    }
}

/* ── Flip-out execution ──────────────────────────────────────────────── */

/**
 * Leave alt-screen, run the EEPROM operation with normal printf output,
 * wait for a keypress, then re-enter alt-screen and set result_msg.
 */
static void gui_execute_operation(void) {
    if (!config_ready()) {
        gui.status_msg = "Configure action + device first";
        return;
    }

    /* Check clock stretching before leaving alt-screen */
    if (i2c_mode_config.clock_stretch) {
        gui.status_msg = "Error: I2C clock stretching enabled";
        gui.result_msg = "Re-enter I2C mode with clock stretching DISABLED.";
        return;
    }

    /* Build the eeprom_info for the operation */
    struct eeprom_info eeprom;
    memset(&eeprom, 0, sizeof(eeprom));
    eeprom.device         = &gui.devices[gui.device_idx];
    eeprom.device_address = gui.i2c_addr;
    eeprom.action         = (uint32_t)gui.action;
    eeprom.verify_flag    = gui.verify_flag;
    strncpy(eeprom.file_name, gui.file_name, sizeof(eeprom.file_name) - 1);

    /* ── Leave alt-screen ── */
    printf("\x1b[?1049l");  /* leave alt screen */
    toolbar_draw_release();
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);

    /* Print chip info */
    printf("%s: %d bytes, %d byte pages, addr %d bytes\r\n\r\n",
           eeprom.device->name, eeprom.device->size_bytes,
           eeprom.device->page_bytes, eeprom.device->address_bytes);

    /* Confirm destructive actions */
    bool confirmed = true;
    if (gui.action == 1 || gui.action == 2 || gui.action == 5) {
        /* erase=1, write=2, test=5 (0-based) */
        printf("This action may modify the EEPROM contents. Continue? (y/n) ");
        char c;
        rx_fifo_get_blocking(&c);
        printf("%c\r\n", c);
        confirmed = (c == 'y' || c == 'Y');
    }

    bool success = false;

    if (confirmed) {
        char buf[EEPROM_ADDRESS_PAGE_SIZE];
        uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE];

        fala_start_hook();

        switch (gui.action) {
        case 0: /* dump */
        {
            struct hex_config_t hex_config;
            ui_hex_init_config(&hex_config);
            hex_config.max_size_bytes = eeprom.device->size_bytes;
            hex_config.start_address = 0;
            hex_config.requested_bytes = eeprom.device->size_bytes;
            hex_config.pager_off = false;
            ui_hex_align_config(&hex_config);
            ui_hex_header_config(&hex_config);
            for (uint32_t i = hex_config._aligned_start;
                 i < (hex_config._aligned_end + 1); i += 16) {
                eeprom.device->hal->read(&eeprom, i, 16, (uint8_t*)buf);
                if (ui_hex_row_config(&hex_config, i, (uint8_t*)buf, 16))
                    break;
            }
            success = true;  /* dump always "succeeds" */
            break;
        }
        case 1: /* erase */
            success = !eeprom_action_erase(&eeprom,
                (uint8_t*)buf, sizeof(buf),
                verify_buf, sizeof(verify_buf),
                gui.verify_flag);
            break;
        case 2: /* write */
            success = !eeprom_action_write(&eeprom,
                (uint8_t*)buf, sizeof(buf),
                verify_buf, sizeof(verify_buf),
                gui.verify_flag);
            break;
        case 3: /* read */
            success = !eeprom_action_read(&eeprom,
                (uint8_t*)buf, sizeof(buf),
                verify_buf, sizeof(verify_buf),
                gui.verify_flag);
            break;
        case 4: /* verify */
            success = !eeprom_action_verify(&eeprom,
                (uint8_t*)buf, sizeof(buf),
                verify_buf, sizeof(verify_buf));
            break;
        case 5: /* test */
            success = !eeprom_action_test(&eeprom,
                (uint8_t*)buf, sizeof(buf),
                verify_buf, sizeof(verify_buf));
            break;
        }

        fala_stop_hook();
        fala_notify_hook();
    }

    /* Show result and wait for keypress */
    if (!confirmed) {
        printf("\r\nAborted.\r\n");
    } else if (success) {
        printf("\r\nSuccess :)\r\n");
    } else {
        printf("\r\nOperation failed.\r\n");
    }
    printf("\r\nPress any key to return to GUI...\r\n");
    { char c; rx_fifo_get_blocking(&c); }

    /* ── Re-enter alt-screen ── */
    toolbar_draw_prepare();
    printf("\x1b[?1049h");  /* enter alt screen */
    printf("\x1b[r");       /* reset scroll region */
    printf("\x1b[2J");      /* clear screen */
    printf("\x1b[H");       /* cursor home */

    /* Drain stale rx */
    { char d; while (rx_fifo_try_get(&d)) {} }

    /* Set result message for the GUI content area */
    if (!confirmed) {
        gui.result_msg = "Aborted by user.";
    } else if (success) {
        gui.result_msg = "Last operation: Success :)";
    } else {
        gui.result_msg = "Last operation: FAILED";
    }
    gui.status_msg = NULL;
}

/* ── Action dispatch ────────────────────────────────────────────────── */

static void gui_dispatch(int action_id) {
    /* Action menu */
    if (action_id >= ACT_DUMP && action_id <= ACT_TEST) {
        gui.action = action_id - ACT_DUMP;  /* maps to 0..5 */
        gui.result_msg = NULL;
        gui.status_msg = NULL;
        return;
    }

    /* Device menu */
    if (action_id >= ACT_DEV_BASE && action_id < ACT_DEV_BASE + gui.device_count) {
        gui.device_idx = action_id - ACT_DEV_BASE;
        gui.result_msg = NULL;
        gui.status_msg = NULL;
        return;
    }

    /* File menu — launch file picker */
    if (action_id == ACT_FILE_NEW) {
        gui_pick_file();
        gui.result_msg = NULL;
        return;
    }

    /* Options menu */
    switch (action_id) {
    case ACT_OPT_VERIFY:
        gui.verify_flag = !gui.verify_flag;
        break;
    case ACT_OPT_ADDR:
        gui_input_i2c_addr();
        break;
    case ACT_OPT_EXECUTE:
        gui_execute_operation();
        break;
    case ACT_OPT_QUIT:
        gui.running = false;
        break;
    }
}

/* ── Key handler (non-menu mode) ────────────────────────────────────── */

static void gui_process_key(int key) {
    switch (key) {
    case VT100_KEY_CTRL_Q:
    case VT100_KEY_ESC:
        gui.running = false;
        break;
    case VT100_KEY_F10:
        gui.menu_pending = true;
        break;
    case VT100_KEY_TAB:
    case VT100_KEY_RIGHT:
        gui.focused_field = focus_next(gui.focused_field);
        gui.status_msg = NULL;
        break;
    case VT100_KEY_LEFT:
        gui.focused_field = focus_prev(gui.focused_field);
        gui.status_msg = NULL;
        break;
    case VT100_KEY_UP:
        gui.status_msg = NULL;
        switch (gui.focused_field) {
        case FOCUS_ACTION:
            gui.action = (gui.action <= 0) ? ACTION_COUNT - 1 : gui.action - 1;
            gui.result_msg = NULL;
            break;
        case FOCUS_DEVICE:
            if (gui.device_count > 0) {
                gui.device_idx = (gui.device_idx <= 0)
                               ? gui.device_count - 1 : gui.device_idx - 1;
                gui.result_msg = NULL;
            }
            break;
        case FOCUS_ADDR:
            gui.i2c_addr = (gui.i2c_addr > 0) ? gui.i2c_addr - 1 : 0x7F;
            break;
        case FOCUS_VERIFY:
            gui.verify_flag = !gui.verify_flag;
            break;
        default:
            break;
        }
        break;
    case VT100_KEY_DOWN:
        gui.status_msg = NULL;
        switch (gui.focused_field) {
        case FOCUS_ACTION:
            gui.action = (gui.action >= ACTION_COUNT - 1) ? 0 : gui.action + 1;
            gui.result_msg = NULL;
            break;
        case FOCUS_DEVICE:
            if (gui.device_count > 0) {
                gui.device_idx = (gui.device_idx >= gui.device_count - 1)
                               ? 0 : gui.device_idx + 1;
                gui.result_msg = NULL;
            }
            break;
        case FOCUS_ADDR:
            gui.i2c_addr = (gui.i2c_addr < 0x7F) ? gui.i2c_addr + 1 : 0;
            break;
        case FOCUS_VERIFY:
            gui.verify_flag = !gui.verify_flag;
            break;
        default:
            break;
        }
        break;
    case ' ':
        if (gui.focused_field == FOCUS_VERIFY) {
            gui.verify_flag = !gui.verify_flag;
        }
        break;
    case VT100_KEY_ENTER:
        gui.status_msg = NULL;
        switch (gui.focused_field) {
        case FOCUS_ACTION:
        case FOCUS_DEVICE:
            /* Enter on spinner fields = advance to next field */
            gui.focused_field = focus_next(gui.focused_field);
            break;
        case FOCUS_FILE:
            gui_pick_file();
            gui.result_msg = NULL;
            break;
        case FOCUS_ADDR:
            gui_input_i2c_addr();
            break;
        case FOCUS_VERIFY:
            gui.verify_flag = !gui.verify_flag;
            break;
        case FOCUS_EXECUTE:
            gui_execute_operation();
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

/* ── Public entry point ─────────────────────────────────────────────── */

bool eeprom_i2c_gui(const struct eeprom_device_t* devices,
                     uint8_t device_count,
                     struct eeprom_info* args) {
    /* ── Init app state ── */
    memset(&gui, 0, sizeof(gui));
    gui.rows         = (uint8_t)system_config.terminal_ansi_rows;
    gui.cols         = (uint8_t)system_config.terminal_ansi_columns;
    gui.running      = true;
    gui.menu_pending = false;
    gui.executed     = false;
    gui.action       = -1;
    gui.device_idx   = -1;
    gui.file_name[0] = 0;
    gui.i2c_addr     = 0x50;
    gui.verify_flag  = false;
    gui.focused_field = FOCUS_ACTION;
    gui.devices      = devices;
    gui.device_count = device_count;
    gui.status_msg   = NULL;  /* use default hint from gui_refresh_screen */
    gui.result_msg   = NULL;

    /* ── Pause toolbar, enter alt-screen ── */
    toolbar_draw_prepare();
    printf("\x1b[?1049h");  /* enter alt screen */
    printf("\x1b[r");       /* reset scroll region */
    printf("\x1b[2J");      /* clear screen */
    printf("\x1b[H");       /* cursor home */

    /* Drain stale rx */
    { char d; while (rx_fifo_try_get(&d)) {} }

    /* Init key decoder */
    vt100_key_init(&gui.keys, gui_read_blocking, gui_read_try);

    /* ── Main loop ── */
    while (gui.running) {
        /* Build dynamic menus fresh each iteration (for checkmarks, etc.) */
        vt100_menu_def_t device_menu  = gui_build_device_menu();
        vt100_menu_def_t file_menu    = gui_build_file_menu();
        vt100_menu_def_t options_menu = gui_build_options_menu();

        vt100_menu_def_t menus[4];
        menus[0] = (vt100_menu_def_t){ "Action", (vt100_menu_item_t*)action_items, ACTION_COUNT };
        menus[1] = device_menu;
        menus[2] = file_menu;
        menus[3] = options_menu;

        vt100_menu_state_t menu_state;
        vt100_menu_init(&menu_state, menus, 4,
                        1, gui.cols, gui.rows,
                        gui_menu_read_key, gui_menu_write);
        menu_state.repaint = gui_repaint_cb;

        /* Refresh screen + draw passive menu bar */
        tx_fifo_wait_drain();
        gui_refresh_screen();
        vt100_menu_draw_bar(&menu_state);

        /* Read one key */
        int key = vt100_key_read(&gui.keys);
        gui_process_key(key);

        /* If menu requested, run the blocking menu loop */
        if (gui.menu_pending) {
            gui.menu_pending = false;

            int action_id = vt100_menu_run(&menu_state);

            if (action_id > 0) {
                gui_dispatch(action_id);
            } else if (action_id == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
                vt100_key_unget(&gui.keys, menu_state.unhandled_key);
            }

            /* Repaint screen — gui_refresh_screen() clears all content
             * rows, so no need for a heavy full-screen clear. */
            gui_write_str("\x1b[0m");
            gui_refresh_screen();
        }
    }

    /* ── Restore terminal ── */
    { char d; while (rx_fifo_try_get(&d)) {} }
    printf("\x1b[?1049l");  /* leave alt screen */
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();

    return true;  /* always "user quit" — execution happens inside the GUI */
}
