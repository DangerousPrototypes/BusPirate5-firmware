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
    bool menu_pending;
    bool executed;         /* true after any operation has run */

    /* Focus: bar vs hex editor */
    uint8_t focus;

    /* Config bar */
    ui_config_bar_t bar;

    /* Status / result text */
    const char *status_msg;
    const char *result_msg;

    /* Hex editor */
    struct editor *hex_ed;
    uint8_t       *arena;
} mem_gui_state_t;

static mem_gui_state_t gui;

/** Button-triggered execute request (set via ui_mem_gui_request_execute). */
static bool exec_requested;

/* ── Operation-area rendering (rows 4–7) ────────────────────────────── */

#define OP_ROW_MSG  4
#define OP_ROW_PROG 5
#define OP_ROW_WARN 6
#define OP_ROW_ERR  7

static void op_clear(void *ctx) {
    (void)ctx;
    char buf[16];
    for (int r = OP_ROW_MSG; r <= OP_ROW_ERR + 1; r++) {
        int n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[K", r);
        ui_app_write_buf(buf, n);
    }
}

static void op_progress(uint32_t cur, uint32_t total, void *ctx) {
    (void)ctx;
    char buf[120];
    int bar_w = (gui.app.cols > 30) ? gui.app.cols - 30 : 20;
    float pct = (total > 0) ? (float)cur / (float)total : 0.0f;
    int filled = (int)(pct * bar_w);

    int n = snprintf(buf, sizeof(buf), "\x1b[%d;3H\x1b[0;36mProgress: [", OP_ROW_PROG);
    ui_app_write_buf(buf, n);
    for (int i = 0; i < bar_w; i++) {
        ui_app_write_str(i < filled ? "#" : " ");
    }
    n = snprintf(buf, sizeof(buf), "] %3d%%\x1b[0m", (int)(pct * 100));
    ui_app_write_buf(buf, n);
}

static void op_message(const char *msg, void *ctx) {
    (void)ctx;
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;3H\x1b[K\x1b[0;1m", OP_ROW_MSG);
    ui_app_write_buf(buf, n);
    while (*msg == '\r' || *msg == '\n') msg++;
    ui_app_write_str(msg);
    ui_app_write_str("\x1b[0m");
}

static void op_error(const char *msg, void *ctx) {
    (void)ctx;
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;3H\x1b[K\x1b[0;1;31m", OP_ROW_ERR);
    ui_app_write_buf(buf, n);
    while (*msg == '\r' || *msg == '\n') msg++;
    ui_app_write_str(msg);
    ui_app_write_str("\x1b[0m");
}

static void op_warning(const char *msg, void *ctx) {
    (void)ctx;
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;3H\x1b[K\x1b[0;33m", OP_ROW_WARN);
    ui_app_write_buf(buf, n);
    while (*msg == '\r' || *msg == '\n') msg++;
    ui_app_write_str(msg);
    ui_app_write_str("\x1b[0m");
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

    /* Row 2: config bar */
    ui_config_bar_draw(&gui.bar);

    /* Row 3: separator with context hints */
    ui_app_write_str("\x1b[3;1H\x1b[0;2m");
    {
        const char *hint;
        if (gui.focus == FOCUS_HEX) {
            hint = "Tab=Config  F10=Menu  :q=Quit editor";
        } else if (gui.status_msg) {
            hint = gui.status_msg;
        } else {
            hint = "Tab=Next  Up/Dn=Change  Enter=Select  F10=Menu";
        }
        int hlen = (int)strlen(hint);
        int pad = gui.app.cols - hlen - 4;
        if (pad < 2) pad = 2;
        ui_app_write_str("--");
        for (int i = 0; i < pad / 2; i++) ui_app_write_str("-");
        ui_app_write_str(" ");
        ui_app_write_str(hint);
        ui_app_write_str(" ");
        int drawn = 2 + pad / 2 + 1 + hlen + 1;
        for (int i = drawn; i < gui.app.cols; i++) ui_app_write_str("-");
    }
    ui_app_write_str("\x1b[0m");

    /* Content area (rows 4+) — only draw fallback when hex editor is not active */
    if (!gui.hex_ed) {
        for (int r = 4; r < gui.app.rows; r++) {
            n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[K", r);
            ui_app_write_buf(buf, n);
        }
        /* Readiness hint */
        bool ready = gui.config->config_ready
                   ? gui.config->config_ready(gui.config->ctx)
                   : true;
        if (!ready) {
            ui_app_write_str("\x1b[5;3H\x1b[0;33m-> Configure all fields, then Execute\x1b[0m");
        } else {
            ui_app_write_str("\x1b[5;3H\x1b[0;1;32mReady! "
                             "\x1b[0mTab to [Execute] and press Enter.");
        }
        if (gui.result_msg) {
            n = snprintf(buf, sizeof(buf), "\x1b[7;3H\x1b[0;32m%s\x1b[0m", gui.result_msg);
            ui_app_write_buf(buf, n);
        }
    }

    ui_app_write_str("\x1b[?25h"); /* show cursor */
}

static void gui_repaint_cb(void) {
    gui_refresh_screen();
    if (gui.hex_ed) {
        editor_refresh_screen(gui.hex_ed);
    }
}

/* ── Options menu builder ───────────────────────────────────────────── */

enum {
    ACT_OPT_EXECUTE = 210,
    ACT_OPT_QUIT    = 211,
};

static vt100_menu_item_t opt_items[4];

static vt100_menu_def_t build_options_menu(void) {
    uint8_t idx = 0;

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
            gui_refresh_screen();
            if (gui.hex_ed) editor_refresh_screen(gui.hex_ed);
            return;
        }
    }

    /* Clear content area */
    op_clear(NULL);

    /* Run the operation */
    fala_start_hook();

    const char *result = NULL;
    /* Cast away const for the mutable ops.ctx field */
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
}

/* ── Key handler ────────────────────────────────────────────────────── */

static void gui_process_key(int key) {
    /* Global keys */
    if (key == VT100_KEY_CTRL_Q || key == VT100_KEY_ESC) {
        gui.running = false;
        return;
    }
    if (key == VT100_KEY_F10) {
        gui.menu_pending = true;
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
    gui.menu_pending = false;
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
        gui.arena = mem_alloc(BIG_BUFFER_SIZE, BP_BIG_BUFFER_EDITOR);
        if (gui.arena) {
            hx_arena_init(gui.arena, BIG_BUFFER_SIZE);
            int hx_exit = setjmp(hx_exit_jmpbuf);
            if (hx_exit != 0) {
                gui.hex_ed = NULL;
                hx_cleanup();
                gui.status_msg = "Hex editor error (out of memory?)";
            } else {
                gui.hex_ed = hx_embed_init(5); /* 3 chrome rows + 2 hx header rows */
            }
        }
    }

    /* ── Main loop ── */
    while (gui.running) {
        /* Build menus */
        vt100_menu_def_t options_menu = build_options_menu();

        /* Assemble menu array: extras + Options */
        uint8_t menu_count = 0;
        vt100_menu_def_t menus[UI_MEM_GUI_MAX_EXTRA_MENUS + 1];

        if (config->extra_menus && config->extra_menu_count > 0) {
            for (uint8_t i = 0; i < config->extra_menu_count &&
                     i < UI_MEM_GUI_MAX_EXTRA_MENUS; i++) {
                menus[menu_count++] = config->extra_menus[i];
            }
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
            editor_refresh_screen(gui.hex_ed);
        }

        /* Hide cursor when on config bar */
        if (gui.focus != FOCUS_HEX) {
            ui_app_write_str("\x1b[?25l");
        }

        /* Draw passive menu bar */
        vt100_menu_draw_bar(&menu_state);

        /* Read and process one key */
        int key = ui_app_read_key(&gui.app);
        gui_process_key(key);

        /* Check for button-triggered execute request */
        if (exec_requested) {
            exec_requested = false;
            gui_execute();
        }

        /* Handle pending menu interaction */
        if (gui.menu_pending) {
            gui.menu_pending = false;
            int action_id = vt100_menu_run(&menu_state);

            if (action_id == ACT_OPT_EXECUTE) {
                gui_execute();
            } else if (action_id == ACT_OPT_QUIT) {
                gui.running = false;
            } else if (action_id > 0 && gui.config->menu_dispatch) {
                gui.config->menu_dispatch(gui.config->ctx, action_id);
            } else if (action_id == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
                ui_app_unget_key(&gui.app, menu_state.unhandled_key);
            }
            /* Forward extra menu action IDs to application dispatch?
             * Not needed — extra menus can trigger field changes via
             * the action IDs which the app processes in field callbacks. */

            ui_app_write_str("\x1b[0m");
            gui_refresh_screen();
            if (gui.hex_ed) {
                editor_refresh_screen(gui.hex_ed);
            }
        }
    }

    /* ── Cleanup ── */
    if (gui.hex_ed) {
        hx_cleanup();
        gui.hex_ed = NULL;
    }
    if (gui.arena) {
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

/* ── Static read_key adapter ────────────────────────────────────────── */

/* vt100_menu_init expects int (*)(void), but ui_app_read_key takes a
 * ui_app_t*.  Since gui is file-static we provide a thin wrapper. */
static int ui_app_read_key_static(void) {
    return ui_app_read_key(&gui.app);
}
