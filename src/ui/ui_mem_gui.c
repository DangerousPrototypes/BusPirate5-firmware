/**
 * @file ui_mem_gui.c
 * @brief Reusable fullscreen GUI template for memory operations.
 *
 * Implements the main loop, menu bar, config bar, hex editor embedding,
 * operation-area rendering, and content area management.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/mem.h"

#include "ui/ui_app.h"
#include "ui/ui_popup.h"
#include "ui/ui_config_bar.h"
#include "ui/ui_mem_gui.h"
#include "ui/ui_file_picker.h"

#include "lib/vt100_menu/vt100_menu.h"
#include "lib/vt100_keys/vt100_keys.h"
#include "lib/hx/hx_compat.h"
#include "lib/hx/editor.h"

#include "binmode/fala.h"

/* Forward declaration for static key adapter */
static int ui_app_read_key_static(void);

/* ── Internal state ─────────────────────────────────────────────────── */

/** Focus targets: the config bar owns FOCUS_BAR, hex editor owns FOCUS_HEX. */
enum {
    FOCUS_BAR = 0,
    FOCUS_HEX = 1,
};

typedef struct {
    /* App scaffold */
    ui_app_t app;

    /* Config */
    const ui_mem_gui_config_t *config;

    /* Run state */
    bool running;
    bool executed;         /* true after any operation has run */

    /* Focus: bar vs hex editor */
    uint8_t focus;

    /* Config bar */
    ui_config_bar_t bar;

    /* Status / result text */
    const char *status_msg;
    const char *result_msg;

    /* Progress popup (used during execute) */
    ui_popup_progress_t progress_popup;

    /* Hex editor */
    struct editor *hex_ed;
    uint8_t       *arena;
} mem_gui_state_t;

static mem_gui_state_t gui;

/** Button-triggered execute request (set via ui_mem_gui_request_execute). */
static bool exec_requested;

/* ── Operation callbacks (route into progress popup) ────────────────── */

static void op_clear(void *ctx)                              { (void)ctx; }
static void op_progress(uint32_t cur, uint32_t total, void *ctx) {
    (void)ctx;
    ui_popup_progress_set_progress(&gui.progress_popup, cur, total);
}
static void op_message(const char *msg, void *ctx) {
    (void)ctx;
    ui_popup_progress_set_message(&gui.progress_popup, msg);
}
static void op_error(const char *msg, void *ctx) {
    (void)ctx;
    ui_popup_progress_set_error(&gui.progress_popup, msg);
}
static void op_warning(const char *msg, void *ctx) {
    (void)ctx;
    ui_popup_progress_set_warning(&gui.progress_popup, msg);
}

static const ui_mem_gui_ops_t gui_ops = {
    .progress = op_progress,
    .message  = op_message,
    .error    = op_error,
    .warning  = op_warning,
    .clear    = op_clear,
    .ctx      = NULL, /* set at call time */
};

/* ── Screen refresh ─────────────────────────────────────────────────── */

static void gui_refresh_screen(void) {
    char buf[120];
    int n;

    ui_app_write_str("\x1b[?25l"); /* hide cursor */

    /* Row 2: config bar (blue toolbar) */
    gui.bar.active = (gui.focus == FOCUS_BAR);
    ui_config_bar_draw(&gui.bar);

    /* Content area (rows 3+) — only draw fallback when hex editor is not active */
    if (!gui.hex_ed) {
        for (int r = 3; r < gui.app.rows; r++) {
            n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[K", r);
            ui_app_write_buf(buf, n);
        }
        /* Readiness hint */
        bool ready = gui.config->config_ready
                   ? gui.config->config_ready(gui.config->ctx)
                   : true;
        if (!ready) {
            ui_app_write_str("\x1b[4;3H\x1b[0;33m-> Configure all fields, then Execute\x1b[0m");
        } else {
            ui_app_write_str("\x1b[4;3H\x1b[0;1;32mReady! "
                             "\x1b[0mTab to [Execute] and press Enter.");
        }
        if (gui.result_msg) {
            n = snprintf(buf, sizeof(buf), "\x1b[6;3H\x1b[0;32m%s\x1b[0m", gui.result_msg);
            ui_app_write_buf(buf, n);
        }
    }

    /* Cursor stays hidden — caller manages visibility */
}

/** Draw hint bar on the bottom row of the terminal. */
static void draw_hint_bar(void) {
    char buf[32];
    int n;
    const char *hint;

    if (gui.focus == FOCUS_HEX) {
        hint = "Tab=Config  :help=Help  :q=Quit editor";
    } else if (gui.status_msg) {
        hint = gui.status_msg;
    } else {
        hint = "L/R=Next  Up/Dn=Change  Enter=Select  Tab=Editor";
    }

    n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[0;37;44m", gui.app.rows);
    ui_app_write_buf(buf, n);
    ui_app_write_str(hint);
    ui_app_write_str("\x1b[K\x1b[0m");
}

static void gui_repaint_cb(void) {
    gui_refresh_screen();
    if (gui.hex_ed) {
        gui.hex_ed->cursor_hidden = (gui.focus != FOCUS_HEX);
        editor_refresh_screen(gui.hex_ed);
    }
    draw_hint_bar();
}

/* ── Options menu builder ───────────────────────────────────────────── */

enum {
    ACT_OPT_EXECUTE = 210,
    ACT_OPT_QUIT    = 211,

    /* Framework-level editor actions */
    ACT_HX_FOCUS    = 250,

    /* Hex editor actions — offset to avoid app action ID conflicts */
    ACT_HX_BASE     = 300,
};

static vt100_menu_item_t opt_items[8];

static vt100_menu_def_t build_options_menu(void) {
    uint8_t idx = 0;

    /* Prepend application-specific option items (e.g. "I2C Address") */
    if (gui.config->option_items && gui.config->option_item_count > 0) {
        for (uint8_t i = 0; i < gui.config->option_item_count && idx < 5; i++) {
            opt_items[idx++] = gui.config->option_items[i];
        }
        /* Separator between app items and framework items */
        opt_items[idx].label     = NULL;
        opt_items[idx].shortcut  = NULL;
        opt_items[idx].action_id = 0;
        opt_items[idx].flags     = MENU_ITEM_SEPARATOR;
        idx++;
    }

    bool ready = gui.config->config_ready
               ? gui.config->config_ready(gui.config->ctx)
               : true;

    opt_items[idx].label     = "Execute";
    opt_items[idx].shortcut  = "Enter";
    opt_items[idx].action_id = ACT_OPT_EXECUTE;
    opt_items[idx].flags     = ready ? 0 : MENU_ITEM_DISABLED;
    idx++;

    /* Separator */
    opt_items[idx].label     = NULL;
    opt_items[idx].shortcut  = NULL;
    opt_items[idx].action_id = 0;
    opt_items[idx].flags     = MENU_ITEM_SEPARATOR;
    idx++;

    opt_items[idx].label     = "Quit";
    opt_items[idx].shortcut  = "^Q";
    opt_items[idx].action_id = ACT_OPT_QUIT;
    opt_items[idx].flags     = 0;
    idx++;

    return (vt100_menu_def_t){ "Options", opt_items, idx };
}

/* ── Editor menu builder (hex commands, always visible) ─────────────── */

static vt100_menu_item_t editor_items[18];

/**
 * Build the "Editor" dropdown.  Items are always present but disabled
 * when the hex editor has no content loaded.
 */
static vt100_menu_def_t build_editor_menu(void) {
    bool has_content = gui.hex_ed && gui.hex_ed->contents;
    bool focused     = has_content && gui.focus == FOCUS_HEX;
    uint8_t dc = has_content ? 0 : MENU_ITEM_DISABLED; /* content gate */
    uint8_t df = focused     ? 0 : MENU_ITEM_DISABLED; /* focus gate  */
    uint8_t idx = 0;

    editor_items[idx++] = (vt100_menu_item_t){ "Focus editor",  "Tab",  ACT_HX_FOCUS,                   dc };
    editor_items[idx++] = (vt100_menu_item_t){ NULL, NULL, 0, MENU_ITEM_SEPARATOR };
    editor_items[idx++] = (vt100_menu_item_t){ "Undo",          "u",    ACT_HX_BASE + HX_ACT_UNDO,     df };
    editor_items[idx++] = (vt100_menu_item_t){ "Redo",          "^R",   ACT_HX_BASE + HX_ACT_REDO,     df };
    editor_items[idx++] = (vt100_menu_item_t){ NULL, NULL, 0, MENU_ITEM_SEPARATOR };
    editor_items[idx++] = (vt100_menu_item_t){ "Insert hex",    "i",    ACT_HX_BASE + HX_ACT_MODE_INS, df };
    editor_items[idx++] = (vt100_menu_item_t){ "Replace hex",   "r",    ACT_HX_BASE + HX_ACT_MODE_REP, df };
    editor_items[idx++] = (vt100_menu_item_t){ "Insert ASCII",  "I",    ACT_HX_BASE + HX_ACT_MODE_INSA,df };
    editor_items[idx++] = (vt100_menu_item_t){ "Replace ASCII", "R",    ACT_HX_BASE + HX_ACT_MODE_REPA,df };
    editor_items[idx++] = (vt100_menu_item_t){ NULL, NULL, 0, MENU_ITEM_SEPARATOR };
    editor_items[idx++] = (vt100_menu_item_t){ "Search",        "/",    ACT_HX_BASE + HX_ACT_SEARCH,   df };
    editor_items[idx++] = (vt100_menu_item_t){ "Next match",    "n",    ACT_HX_BASE + HX_ACT_NEXT,     df };
    editor_items[idx++] = (vt100_menu_item_t){ "Go to offset",  ":",    ACT_HX_BASE + HX_ACT_GOTO,     df };
    editor_items[idx++] = (vt100_menu_item_t){ NULL, NULL, 0, MENU_ITEM_SEPARATOR };
    editor_items[idx++] = (vt100_menu_item_t){ "Help",          ":help",ACT_HX_BASE + HX_ACT_HELP,     dc };

    return (vt100_menu_def_t){ "Editor", editor_items, idx };
}

/* ── Execute operation ──────────────────────────────────────────────── */

static void gui_execute(void) {
    const ui_mem_gui_config_t *cfg = gui.config;

    /* Pre-check */
    if (cfg->pre_check) {
        const char *err = NULL;
        if (!cfg->pre_check(cfg->ctx, &err)) {
            gui.status_msg = err ? err : "Pre-check failed";
            return;
        }
    }

    /* Config readiness */
    if (cfg->config_ready && !cfg->config_ready(cfg->ctx)) {
        gui.status_msg = "Configure all fields first";
        return;
    }

    /* Confirm destructive operations */
    if (cfg->needs_confirm && cfg->needs_confirm(cfg->ctx)) {
        ui_popup_io_t pio = {
            .write_out = ui_app_write_out_cb,
            .cols = gui.app.cols,
            .rows = gui.app.rows,
        };
        if (!ui_popup_confirm(&pio, NULL,
                              "This may modify device contents!",
                              UI_POPUP_DANGER)) {
            gui.result_msg = "Aborted by user.";
            gui.status_msg = NULL;
            gui_repaint_cb();
            return;
        }
    }

    /* Open a progress popup that persists for the entire operation */
    ui_popup_io_t pio = {
        .write_out = ui_app_write_out_cb,
        .cols = gui.app.cols,
        .rows = gui.app.rows,
    };
    ui_popup_progress_open(&gui.progress_popup, &pio, cfg->title);

    /* Run the operation */
    fala_start_hook();

    const char *result = NULL;
    ui_mem_gui_ops_t ops = gui_ops;
    ops.ctx = cfg->ctx;

    bool ok = cfg->execute(cfg->ctx, &ops, &result);

    fala_stop_hook();
    fala_notify_hook();

    /* Load results into hex editor if applicable */
    if (ok && gui.hex_ed && cfg->post_execute_load) {
        cfg->post_execute_load(cfg->ctx);
    }

    gui.result_msg = result ? result : (ok ? "Success :)" : "Operation FAILED");
    gui.status_msg = NULL;
    gui.executed = true;

    /* Show result in the popup and wait for user dismissal */
    ui_popup_progress_finish(&gui.progress_popup, ok, gui.result_msg);
    ui_popup_progress_wait(&gui.progress_popup);

    /* Repaint full screen after popup closes */
    gui_repaint_cb();
}

/* ── Key handler ────────────────────────────────────────────────────── */

static void gui_process_key(int key) {
    /* Global keys — Ctrl-Q quits; ESC is intentionally ignored here
     * because it is also the prefix/cancel key for popups and the menu
     * bar, so a stray press would discard the user's work. */
    if (key == VT100_KEY_CTRL_Q) {
        gui.running = false;
        return;
    }

    /* Tab switches between config bar and hex editor */
    if (key == VT100_KEY_TAB) {
        if (gui.focus == FOCUS_HEX) {
            gui.focus = FOCUS_BAR;
        } else if (gui.hex_ed && gui.hex_ed->contents) {
            /* Only switch to hex if there's content loaded */
            gui.focus = FOCUS_HEX;
        } else {
            /* No hex editor — cycle within config bar */
            ui_config_bar_focus_next(&gui.bar);
        }
        gui.status_msg = NULL;
        return;
    }

    /* Hex editor focus: forward all other keys to hx */
    if (gui.focus == FOCUS_HEX && gui.hex_ed) {
        read_key_unget(key);
        editor_process_keypress(gui.hex_ed);
        if (gui.hex_ed->quit_requested) {
            gui.hex_ed->quit_requested = false;
            gui.focus = FOCUS_BAR;
        }
        return;
    }

    /* Config bar handles arrows, Enter, Space */
    if (ui_config_bar_handle_key(&gui.bar, key)) {
        gui.status_msg = NULL;
        return;
    }
}

/* ── Public entry point ─────────────────────────────────────────────── */

bool ui_mem_gui_run(const ui_mem_gui_config_t *config) {
    /* ── Init state ── */
    memset(&gui, 0, sizeof(gui));
    gui.config       = config;
    gui.running      = true;
    gui.executed     = false;
    gui.focus        = FOCUS_BAR;
    gui.status_msg   = NULL;
    gui.result_msg   = NULL;
    gui.hex_ed       = NULL;
    gui.arena        = NULL;
    exec_requested   = false;

    /* Open fullscreen */
    ui_app_open(&gui.app);

    /* Init config bar */
    ui_config_bar_init(&gui.bar, config->fields, config->field_count,
                       config->ctx, 2, gui.app.cols, gui.app.rows);
    gui.bar.write_out = ui_app_write_out_cb;
    gui.bar.read_key  = ui_app_read_key_static;
    gui.bar.repaint   = gui_repaint_cb;

    /* Allocate arena and init hex editor */
    if (config->enable_hex_editor) {
        if (config->arena_buf) {
            /* App pre-allocated the big buffer and carved out scratch */
            gui.arena = config->arena_buf;
            hx_arena_init(gui.arena, config->arena_size);
        } else {
            gui.arena = mem_alloc(BIG_BUFFER_SIZE, BP_BIG_BUFFER_EDITOR);
            if (gui.arena)
                hx_arena_init(gui.arena, BIG_BUFFER_SIZE);
        }
        if (gui.arena) {
            int hx_exit = setjmp(hx_exit_jmpbuf);
            if (hx_exit != 0) {
                gui.hex_ed = NULL;
                hx_cleanup();
                gui.status_msg = "Hex editor error (out of memory?)";
            } else {
                gui.hex_ed = hx_embed_init(4); /* 2 chrome rows + 2 hx header rows */
                /* Reserve the last terminal row for the hint bar so it
                 * doesn't overwrite the hex editor's status/command line. */
                gui.hex_ed->screen_rows -= 1;
            }
        }
    }

    /* ── Main loop ── */
    while (gui.running) {
        /* Build menus */
        vt100_menu_def_t options_menu = build_options_menu();

        /* Assemble menu array: extras + Editor + Options */
        uint8_t menu_count = 0;
        vt100_menu_def_t menus[UI_MEM_GUI_MAX_EXTRA_MENUS + 2];

        if (config->extra_menus && config->extra_menu_count > 0) {
            for (uint8_t i = 0; i < config->extra_menu_count &&
                     i < UI_MEM_GUI_MAX_EXTRA_MENUS; i++) {
                menus[menu_count++] = config->extra_menus[i];
            }
        }
        if (config->enable_hex_editor) {
            menus[menu_count++] = build_editor_menu();
        }
        menus[menu_count++] = options_menu;

        vt100_menu_state_t menu_state;
        vt100_menu_init(&menu_state, menus, menu_count,
                        1, gui.app.cols, gui.app.rows,
                        (int (*)(void))&ui_app_read_key_static,
                        ui_app_write_out_cb);
        menu_state.repaint = gui_repaint_cb;

        /* Refresh screen */
        tx_fifo_wait_drain();
        gui_refresh_screen();

        /* Render hex editor content area */
        if (gui.hex_ed) {
            gui.hex_ed->cursor_hidden = (gui.focus != FOCUS_HEX);
            editor_refresh_screen(gui.hex_ed);
        }
        draw_hint_bar();

        /* Cursor is always hidden — hex editor uses reverse-video
         * to highlight the active byte; command/search modes in the
         * hex editor show cursor themselves via their charbuf. */
        ui_app_write_str("\x1b[?25l");

        /* Draw passive menu bar */
        vt100_menu_draw_bar(&menu_state);

        /* Read and process one key */
        int key = ui_app_read_key(&gui.app);

        if (vt100_menu_check_trigger(&menu_state, key)) {
            int action_id = vt100_menu_run(&menu_state);

            if (action_id == ACT_OPT_EXECUTE) {
                gui_execute();
            } else if (action_id == ACT_OPT_QUIT) {
                gui.running = false;
            } else if (action_id == ACT_HX_FOCUS && gui.hex_ed && gui.hex_ed->contents) {
                gui.focus = FOCUS_HEX;
            } else if (action_id >= ACT_HX_BASE && gui.hex_ed) {
                /* Hex editor menu action — strip offset, dispatch */
                hx_menu_dispatch(gui.hex_ed, action_id - ACT_HX_BASE);
                if (gui.hex_ed->quit_requested) {
                    gui.hex_ed->quit_requested = false;
                    gui.focus = FOCUS_BAR;
                }
            } else if (action_id > 0 && gui.config->menu_dispatch) {
                gui.config->menu_dispatch(gui.config->ctx, action_id);
            } else if (action_id == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
                ui_app_unget_key(&gui.app, menu_state.unhandled_key);
            }

            ui_app_write_str("\x1b[0m");
            ui_app_write_str("\x1b[?25l"); /* hide cursor before redraw */
            gui_repaint_cb();
        } else {
            gui_process_key(key);

            /* Check for button-triggered execute request */
            if (exec_requested) {
                exec_requested = false;
                gui_execute();
            }
        }
    }

    /* ── Cleanup ── */
    if (gui.hex_ed) {
        hx_cleanup();
        gui.hex_ed = NULL;
    }
    if (gui.arena && !config->arena_buf) {
        mem_free(gui.arena);
        gui.arena = NULL;
    }

    ui_app_close(&gui.app);

    return gui.executed;
}

/* ── Public: request execute from button on_activate ────────────────── */

void ui_mem_gui_request_execute(void) {
    exec_requested = true;
}

/* ── Public helpers for app callbacks ────────────────────────────────── */

bool ui_mem_gui_browse_file(char *buf, uint8_t buf_size) {
    ui_file_picker_io_t fpio = {
        .read_key  = ui_app_read_key_static,
        .write_out = ui_app_write_out_cb,
        .repaint   = gui_repaint_cb,
        .cols      = gui.app.cols,
        .rows      = gui.app.rows,
    };
    bool ok = ui_file_pick(NULL, buf, buf_size, &fpio);
    gui_repaint_cb();
    return ok;
}

bool ui_mem_gui_popup_number(const char *title, uint32_t current,
                             uint32_t min, uint32_t max, uint32_t *result) {
    ui_popup_io_t pio = {
        .write_out = ui_app_write_out_cb,
        .cols = gui.app.cols,
        .rows = gui.app.rows,
    };
    bool ok = ui_popup_number(&pio, title, current, min, max, result);
    gui_repaint_cb();
    return ok;
}

/* ── Static read_key adapter ────────────────────────────────────────── */

/* vt100_menu_init expects int (*)(void), but ui_app_read_key takes a
 * ui_app_t*.  Since gui is file-static we provide a thin wrapper. */
static int ui_app_read_key_static(void) {
    return ui_app_read_key(&gui.app);
}
