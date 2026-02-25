/**
 * @file ui_toolbar.c
 * @brief Central toolbar registry and layout manager implementation.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "usb_tx.h"
#include "tusb.h"
#include "pirate/intercore_helpers.h"

static toolbar_t* toolbar_registry[TOOLBAR_MAX_COUNT];
static uint8_t toolbar_count = 0;

/* ── Core1 state (declared early so toolbar_draw_prepare can reference) ───── */

/**
 * @brief Core1 toolbar-update states.
 * @details IDLE → RENDERING → DRAINING → RENDERING → … → IDLE
 */
enum {
    TB_C1_IDLE,       ///< No update in progress
    TB_C1_RENDERING,  ///< Finding and rendering the next toolbar
    TB_C1_DRAINING,   ///< Waiting for tx_tb_buf to drain to USB
};

static uint8_t  tb_c1_state        = TB_C1_IDLE;
static uint8_t  tb_c1_index        = 0;
static uint32_t tb_c1_update_flags = 0;

bool toolbar_register(toolbar_t* tb) {
    if (toolbar_count >= TOOLBAR_MAX_COUNT) {
        return false;
    }
    if (tb->anchor_bottom) {
        /* Insert at index 0 — bottommost position on screen */
        for (uint8_t i = toolbar_count; i > 0; i--) {
            toolbar_registry[i] = toolbar_registry[i - 1];
        }
        toolbar_registry[0] = tb;
    } else {
        toolbar_registry[toolbar_count] = tb;
    }
    toolbar_count++;
    return true;
}

void toolbar_unregister(toolbar_t* tb) {
    for (uint8_t i = 0; i < toolbar_count; i++) {
        if (toolbar_registry[i] == tb) {
            if (tb->destroy) {
                tb->destroy(tb);
            }
            // Shift remaining entries down
            for (uint8_t j = i; j < toolbar_count - 1; j++) {
                toolbar_registry[j] = toolbar_registry[j + 1];
            }
            toolbar_registry[--toolbar_count] = NULL;
            return;
        }
    }
}

bool toolbar_activate(toolbar_t* tb) {
    tb->enabled = true;
    if (!toolbar_register(tb)) {
        tb->enabled = false;
        return false;
    }
    toolbar_apply_scroll_region();
    toolbar_redraw_all();
    return true;
}

void toolbar_teardown(toolbar_t* tb) {
    if (!tb->enabled) {
        return;
    }
    toolbar_draw_prepare();
    ui_term_cursor_save();

    /* Erase the entire toolbar area (old layout, before unregister) */
    uint16_t rows = system_config.terminal_ansi_rows;
    uint16_t old_top = toolbar_scroll_bottom() + 1;
    for (uint16_t r = old_top; r <= rows; r++) {
        ui_term_cursor_position(r, 0);
        ui_term_erase_line();
    }

    toolbar_unregister(tb);
    tb->enabled = false;
    toolbar_apply_scroll_region();

    /* Redraw all remaining toolbars at their new positions */
    uint16_t width = system_config.terminal_ansi_columns;
    for (uint8_t i = 0; i < toolbar_count; i++) {
        toolbar_t* t = toolbar_registry[i];
        if (t->enabled && t->draw) {
            uint16_t start_row = toolbar_get_start_row(t);
            t->draw(t, start_row, width);
        }
    }

    ui_term_cursor_restore();
    toolbar_draw_release();
}

void toolbar_teardown_all(void) {
    if (toolbar_count == 0) {
        return;
    }
    toolbar_draw_prepare();
    ui_term_cursor_save();

    /* Erase the entire toolbar area */
    uint16_t rows = system_config.terminal_ansi_rows;
    uint16_t old_top = toolbar_scroll_bottom() + 1;
    for (uint16_t r = old_top; r <= rows; r++) {
        ui_term_cursor_position(r, 0);
        ui_term_erase_line();
    }

    /* Tear down each toolbar (reverse order so indices stay valid) */
    while (toolbar_count > 0) {
        toolbar_t* tb = toolbar_registry[toolbar_count - 1];
        if (tb->destroy) {
            tb->destroy(tb);
        }
        tb->enabled = false;
        toolbar_registry[--toolbar_count] = NULL;
    }

    /* Restore full-screen scroll region */
    toolbar_apply_scroll_region();

    ui_term_cursor_restore();
    toolbar_draw_release();
}

uint16_t toolbar_total_height(void) {
    uint16_t total = 0;
    for (uint8_t i = 0; i < toolbar_count; i++) {
        if (toolbar_registry[i]->enabled) {
            total += toolbar_registry[i]->height;
        }
    }
    return total;
}

uint8_t toolbar_count_registered(void) {
    return toolbar_count;
}

uint16_t toolbar_scroll_bottom(void) {
    uint16_t rows = system_config.terminal_ansi_rows;
    uint16_t total = toolbar_total_height();
    if (total >= rows) {
        return 1; // degenerate: no scroll area
    }
    return rows - total;
}

uint16_t toolbar_get_start_row(const toolbar_t* tb) {
    uint16_t row = system_config.terminal_ansi_rows;

    // Walk forward — first registered = bottommost (statusbar stays at bottom)
    for (uint8_t i = 0; i < toolbar_count; i++) {
        if (!toolbar_registry[i]->enabled) {
            continue;
        }
        row -= toolbar_registry[i]->height;
        if (toolbar_registry[i] == tb) {
            return row + 1; // convert to 1-based
        }
    }
    return 0; // not found / disabled
}

void toolbar_apply_scroll_region(void) {
    if (!system_config.terminal_ansi_color) {
        return;
    }
    uint16_t bottom = toolbar_scroll_bottom();
    ui_term_scroll_region(1, bottom);
}

void toolbar_redraw_all(void) {
    toolbar_draw_prepare();
    ui_term_cursor_save();
    uint16_t width = system_config.terminal_ansi_columns;
    for (uint8_t i = 0; i < toolbar_count; i++) {
        toolbar_t* tb = toolbar_registry[i];
        if (tb->enabled && tb->draw) {
            uint16_t start_row = toolbar_get_start_row(tb);
            tb->draw(tb, start_row, width);
        }
    }
    ui_term_cursor_restore();
    toolbar_draw_release();
}

void toolbar_draw_prepare(void) {
    system_config.terminal_toolbar_pause = true;
    /* Spin until Core1 finishes any in-progress render cycle.
     * Setting toolbar_pause above prevents new cycles from starting;
     * we just need to wait for a running one to reach TB_C1_IDLE. */
    while (tb_c1_state != TB_C1_IDLE) {
        tight_loop_contents();
    }
    system_config.terminal_hide_cursor = true;
    printf("%s", ui_term_cursor_hide());
}

void toolbar_draw_release(void) {
    system_config.terminal_hide_cursor = false;
    printf("%s", ui_term_cursor_show());
    system_config.terminal_toolbar_pause = false;
}

void toolbar_pause_updates(void) {
    system_config.terminal_toolbar_pause = true;
}

void toolbar_resume_updates(void) {
    system_config.terminal_toolbar_pause = false;
}

void toolbar_print_registry(void) {
    printf("Toolbar registry: %u / %u slots used\r\n", toolbar_count, TOOLBAR_MAX_COUNT);
    printf("Terminal: %u rows x %u cols, scroll 1..%u\r\n",
           system_config.terminal_ansi_rows,
           system_config.terminal_ansi_columns,
           toolbar_scroll_bottom());
    if (toolbar_count == 0) {
        printf("  (empty)\r\n");
        return;
    }
    for (uint8_t i = 0; i < toolbar_count; i++) {
        toolbar_t* tb = toolbar_registry[i];
        uint16_t start = toolbar_get_start_row(tb);
        printf("  [%u] \"%s\"  height=%u  enabled=%u  row=%u..%u  draw=%s  core1=%s\r\n",
               i, tb->name, tb->height, tb->enabled,
               start, start + tb->height - 1,
               tb->draw ? "yes" : "no",
               tb->update_core1 ? "yes" : "no");
    }
}
/* ── Core0 blocking update request ──────────────────────────────────────────── */

void toolbar_update_blocking(void) {
    BP_ASSERT_CORE0();
    if (!tud_cdc_n_connected(0)) return;
    icm_core0_send_message_synchronous(BP_ICM_UPDATE_TOOLBARS);
}
/* ── Core1 cooperative state machine ──────────────────────────────────── */

void toolbar_core1_begin_update(uint32_t update_flags) {
    if (tb_c1_state != TB_C1_IDLE) {
        return; /* previous cycle still running — skip this tick */
    }
    tb_c1_update_flags = update_flags;
    tb_c1_index = 0;
    tb_c1_state = TB_C1_RENDERING;
}

void toolbar_core1_service(void) {
    if (tb_c1_state == TB_C1_IDLE) {
        return;
    }

    /* DRAINING: wait for tx_fifo_service() to finish sending the buffer. */
    if (tb_c1_state == TB_C1_DRAINING) {
        if (tx_tb_buf_ready) {
            return; /* still draining — come back next loop iteration */
        }
        tb_c1_index++;
        tb_c1_state = TB_C1_RENDERING;
        /* fall through to RENDERING */
    }

    /* RENDERING: find the next toolbar with a Core1 callback and render it. */
    if (!tud_cdc_n_connected(0)) {
        tb_c1_state = TB_C1_IDLE;
        return; /* no USB connection — abort cycle */
    }

    while (tb_c1_index < toolbar_count) {
        toolbar_t* tb = toolbar_registry[tb_c1_index];

        if (tb->enabled && tb->update_core1) {
            uint16_t start_row = toolbar_get_start_row(tb);
            if (start_row == 0) {
                tb_c1_index++;
                continue;
            }

            uint16_t width   = system_config.terminal_ansi_columns;
            size_t   buf_len = sizeof(tx_tb_buf);
            uint32_t len     = 0;

            /* Cursor envelope: save + hide */
            len += ui_term_cursor_save_buf(&tx_tb_buf[len], buf_len - len);
            len += ui_term_cursor_hide_buf(&tx_tb_buf[len], buf_len - len);

            /* Let the toolbar render its content */
            uint32_t content = tb->update_core1(
                tb, &tx_tb_buf[len], buf_len - len,
                start_row, width, tb_c1_update_flags);

            if (content == 0) {
                /* Callback chose not to render — skip to next toolbar */
                tb_c1_index++;
                continue;
            }
            len += content;

            /* Cursor envelope: restore + show */
            len += ui_term_cursor_restore_buf(&tx_tb_buf[len], buf_len - len);
            if (!system_config.terminal_hide_cursor) {
                len += ui_term_cursor_show_buf(&tx_tb_buf[len], buf_len - len);
            }

            // additional check in case USB was disconnected while we were rendering
            if (!tud_cdc_n_connected(0)) {
                tb_c1_state = TB_C1_IDLE;
                return; /* no USB connection — abort cycle */
            }
            tx_tb_start(len);
            tb_c1_state = TB_C1_DRAINING;
            return;
        }
        tb_c1_index++;
    }

    /* All toolbars processed — cycle complete. */
    tb_c1_state = TB_C1_IDLE;
}
