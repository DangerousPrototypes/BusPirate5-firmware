/**
 * @file pin_watcher.c
 * @brief 2-line GPIO pin state watcher toolbar — REFERENCE IMPLEMENTATION.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 *  THIS FILE IS THE REFERENCE IMPLEMENTATION FOR NEW TOOLBARS.
 *  See docs/new_toolbar_guide.md for the step-by-step walkthrough.
 *  See also: src/toolbars/sys_stats.c for a simpler 1-line example.
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * @details
 *  This toolbar displays two rows at the bottom of the VT100 terminal:
 *    Row 1: Colored pin labels  (IO0 .. IO7)
 *    Row 2: Live HIGH/LOW state with matching colors
 *
 *  ## Architecture Overview
 *
 *  The Bus Pirate is a dual-core RP2040 system:
 *    - **Core0** runs commands, printf, and user interaction
 *    - **Core1** runs USB TX and periodic toolbar rendering
 *
 *  Toolbars can render two ways:
 *    1. **Core0 printf path** — good for event-driven or one-shot toolbars
 *       (e.g. test_toolbar, logic_bar). The .draw callback uses printf().
 *    2. **Core1 _buf() path** — good for periodic/live-data toolbars
 *       (e.g. this file, sys_stats, statusbar). The .update_core1 callback
 *       writes VT100 content into a shared buffer using snprintf/_buf()
 *       variants. Core1 sends it atomically over USB, avoiding interleaving
 *       with printf output.
 *
 *  This file uses the Core1 path. The .draw callback is a one-liner that
 *  delegates to toolbar_update_blocking(), which signals Core1 to call our
 *  .update_core1 callback. This gives us a single rendering function that
 *  handles both the initial paint and periodic refresh.
 *
 *  ## Lifecycle
 *
 *  1. pin_watcher_start()  — push lines, toolbar_activate(), reposition cursor
 *  2. Core1 periodic ticks — calls .update_core1 every ~100ms with update_flags
 *  3. pin_watcher_stop()   — toolbar_teardown() erases & unregisters
 *
 *  ## Key Rules for Core1 Rendering
 *
 *  - NO printf() — only snprintf() into the provided buffer
 *  - NO ui_term_*() calls — only _buf() variants (e.g. ui_term_cursor_position_buf)
 *  - Column-pad with spaces instead of erase_line (avoids flicker)
 *  - Return 0 to skip a cycle (nothing changed), >0 to send the buffer
 *  - Cursor save/hide/restore/show is handled by the caller (ui_toolbar.c)
 */

/* ── Step 1: Includes ────────────────────────────────────────────────────────
 *
 * Every toolbar needs these core headers.  Add others as needed for your
 * specific hardware access.
 */
#include <stdio.h>                 // printf (for start/stop messages, NOT for Core1 rendering)
#include <stdint.h>                // uint8_t, uint16_t, uint32_t
#include <stdbool.h>               // bool
#include "pico/stdlib.h"           // RP2040 SDK — busy_wait, gpio, etc.
#include "pirate.h"                // Global defines, BIO_MAX_PINS, hw_pin_label_ordered, colors
#include "system_config.h"         // system_config struct (terminal size, etc.)
#include "pirate/bio.h"            // bio_get() — read GPIO pin state
#include "ui/ui_term.h"            // VT100 helpers: cursor_position_buf, color_buf, color_reset
#include "ui/ui_toolbar.h"         // Toolbar API: toolbar_t, toolbar_activate, toolbar_teardown, etc.
#include "ui/ui_flags.h"           // UI_UPDATE_* flags for selective rendering

/* ── Step 2: Height Constant ─────────────────────────────────────────────────
 *
 * Every toolbar must declare its fixed height.  This tells the toolbar
 * registry how many terminal rows to reserve.  The registry uses this to:
 *   - Calculate the scroll region bottom (toolbar_scroll_bottom())
 *   - Calculate each toolbar's starting row (toolbar_get_start_row())
 *
 * The height is set once in the toolbar_t struct and must not change while
 * the toolbar is registered.
 */
#define PIN_WATCHER_HEIGHT 2

/* ── Forward Declarations ────────────────────────────────────────────────────
 *
 * The Core1 callback needs to be declared before the toolbar_t struct
 * initializer references it.  The signature is fixed by the toolbar API:
 *
 *   uint32_t callback(toolbar_t* tb, char* buf, size_t buf_len,
 *                     uint16_t start_row, uint16_t width,
 *                     uint32_t update_flags);
 *
 * Returns: number of bytes written to buf, or 0 to skip this cycle.
 */
static uint32_t pin_watcher_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                            uint16_t start_row, uint16_t width,
                                            uint32_t update_flags);

/* ── Step 3: The .draw Callback ──────────────────────────────────────────────
 *
 * .draw is called from Core0 during toolbar_redraw_all() and toolbar_teardown().
 * It is responsible for painting the toolbar's content.
 *
 * For Core1-rendered toolbars (like this one), set .draw = NULL.  The framework
 * auto-detects .update_core1 and calls toolbar_update_blocking() on your behalf.
 *
 * For Core0-only toolbars (like test_toolbar or logic_bar), .draw should contain
 * the actual printf rendering code.
 *
 * IMPORTANT: .draw is called inside a prepare/release envelope — cursor is
 * already saved and hidden, toolbar_pause is set.  Do NOT call
 * toolbar_draw_prepare()/toolbar_draw_release() from here.
 *
 * ── Step 4: The toolbar_t Struct ────────────────────────────────────────────
 *
 * This is the toolbar descriptor — the single source of truth for how this
 * toolbar integrates with the registry.  It is file-static and owned by this
 * module.  The pointer must remain valid for the entire time the toolbar is
 * registered.
 *
 * Field reference:
 *   .name          — Human-readable identifier, shown by `toolbar list`
 *   .height        — Number of terminal rows (must match your rendering)
 *   .enabled       — Managed by toolbar_activate()/toolbar_teardown(), do not set manually
 *   .anchor_bottom — If true, this toolbar is inserted at index 0 (bottommost on screen).
 *                    Used by the statusbar to always stay at the very bottom.
 *                    Most toolbars should leave this false.
 *   .owner_data    — Opaque pointer for toolbar-private state.  Useful if you
 *                    need to share state between callbacks without file-scope globals.
 *                    NULL if not needed (like this toolbar).
 *   .draw          — Core0 full-paint callback (see Step 3 above).
 *                    For Core1-rendered toolbars, set to NULL — the framework
 *                    auto-detects .update_core1 and delegates via
 *                    toolbar_update_blocking().
 *                    For Core0-only toolbars (test_toolbar, logic_bar), provide
 *                    a function that paints using printf.
 *   .update_core1  — Core1 periodic rendering callback (see Step 7 below).
 *                    Must use only _buf() variants and snprintf — no printf.
 *                    Set to NULL for Core0-only toolbars.
 *   .destroy       — Called on unregister.  Free resources, stop timers, etc.
 *                    NULL if no cleanup is needed.
 */
static const toolbar_def_t pin_watcher_toolbar_def = {
    .name         = "pin_watcher",
    .height       = PIN_WATCHER_HEIGHT,
    .anchor_bottom = false,      // stacks above statusbar in registration order
    .draw         = NULL, /* Core1-rendered: toolbar_redraw_all() auto-delegates */
    .update_core1 = pin_watcher_update_core1_cb,
    .destroy      = NULL,        // no cleanup needed
};

static toolbar_t pin_watcher_toolbar = {
    .def        = &pin_watcher_toolbar_def,
    .height     = PIN_WATCHER_HEIGHT,
    .enabled    = false,
    .owner_data = NULL,
};

/* ── Step 5: Start / Stop / Is_Active Lifecycle ──────────────────────────────
 *
 * Every toolbar exposes a public API with at least start, stop, and is_active.
 * These are the only functions declared in the header file.
 *
 * ### Start Pattern
 *
 *   1. Guard: if already enabled, return early
 *   2. Call toolbar_activate() — this handles everything:
 *      - Pushes \r\n × height to scroll content up and make room
 *      - Registers the toolbar in the registry
 *      - Recalculates the scroll region
 *      - Calls toolbar_redraw_all() to paint all toolbars
 *      - Repositions the cursor at toolbar_scroll_bottom()
 *
 * ### Stop Pattern
 *
 *   1. Guard: if not enabled, return early
 *   2. Call toolbar_teardown() — this erases the toolbar area, unregisters,
 *      recalculates scroll region, and redraws remaining toolbars.
 *      The function handles everything; no manual cleanup needed.
 */
bool pin_watcher_start(void) {
    if (pin_watcher_toolbar.enabled) {
        return true; /* already active */
    }
    return toolbar_activate(&pin_watcher_toolbar);
}

void pin_watcher_stop(void) {
    if (!pin_watcher_toolbar.enabled) {
        return;
    }
    toolbar_teardown(&pin_watcher_toolbar);
}

bool pin_watcher_is_active(void) {
    return pin_watcher_toolbar.enabled;
}

/* ── Step 6: On-Demand Update ────────────────────────────────────────────────
 *
 * Toolbars with Core1 rendering get periodic updates automatically from the
 * LCD tick cycle (~100ms).  But sometimes you want to force an immediate
 * refresh from Core0 — for example, after a command changes pin directions.
 *
 * toolbar_update_blocking() sends an intercore message to Core1, which calls
 * .update_core1 with UI_UPDATE_ALL flags.  It blocks until Core1 finishes
 * rendering and the buffer starts draining.
 *
 * For Core0-only toolbars, call toolbar_redraw_all() instead (or your own
 * draw function wrapped in toolbar_draw_prepare/release).
 */
void pin_watcher_update(void) {
    if (!pin_watcher_toolbar.enabled) {
        return;
    }
    toolbar_update_blocking();
}

/* ── Step 7: Core1 Rendering Callback ────────────────────────────────────────
 *
 * This is the heart of a periodic toolbar.  It is called from the Core1
 * state machine (toolbar_core1_service) with a pre-allocated buffer and
 * the toolbar's screen coordinates.
 *
 * ### Parameters
 *   tb           — pointer to this toolbar (access .height, .owner_data, etc.)
 *   buf          — character buffer to write VT100 escape sequences + text into
 *   buf_len      — total available bytes in buf (currently 1024 shared across all toolbars)
 *   start_row    — first terminal row (1-based) assigned to this toolbar
 *   width        — terminal width in columns
 *   update_flags — bitmask of UI_UPDATE_* flags indicating what changed:
 *                  UI_UPDATE_VOLTAGES — ADC readings changed
 *                  UI_UPDATE_LABELS   — pin names/modes changed
 *                  UI_UPDATE_FORCE    — force full repaint (e.g. from toolbar_update_blocking)
 *                  UI_UPDATE_CURRENT  — PSU current changed
 *                  UI_UPDATE_ALL      — all of the above (used by blocking path)
 *
 * ### Return Value
 *   0     — nothing to send this cycle (skip).  The state machine moves to
 *           the next toolbar without touching USB.
 *   >0    — number of bytes written.  The state machine wraps your content
 *           in a cursor save/hide + restore/show envelope and sends it via
 *           the tx_tb_buf USB path.
 *
 * ### Rules
 *   1. NO printf() — you are on Core1, printf goes through Core0's SPSC queue
 *   2. Use snprintf(&buf[len], buf_len - len, ...) for text
 *   3. Use _buf() variants for VT100 sequences (ui_term_cursor_position_buf, etc.)
 *   4. Pad with spaces instead of using erase_line — erase causes flicker
 *      because it momentarily blanks the line before your content appears
 *   5. Track your column count and pad to `width` to overwrite stale content
 *   6. The cursor envelope (save/hide/restore/show) is handled by the caller —
 *      do NOT emit cursor save/restore in your callback
 *   7. Always reset colors at the end of your output (ui_term_color_reset)
 *
 * ### Selective Rendering with update_flags
 *
 * The update_flags bitmask tells you what changed since the last tick.
 * Use it to skip expensive rendering when nothing relevant changed:
 *
 *   - Define a care_mask of the flags your toolbar responds to
 *   - Return 0 immediately if (update_flags & care_mask) == 0
 *   - For multi-row toolbars, render static rows (labels) only on FORCE/LABELS
 *   - For single-row toolbars (sys_stats), you can ignore flags and always render
 *     (uptime changes every tick anyway)
 */
static uint32_t pin_watcher_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                            uint16_t start_row, uint16_t width,
                                            uint32_t update_flags) {
    (void)tb;
    uint32_t len = 0;

    /*
     * Gate: only render when something we care about changed.
     * - UI_UPDATE_VOLTAGES — pin voltage readings updated (implies state may have changed)
     * - UI_UPDATE_LABELS   — pin names/configuration changed (need full repaint)
     * - UI_UPDATE_FORCE    — explicit full repaint request (e.g. toolbar_update_blocking)
     *
     * If none of these flags are set, return 0 to skip this cycle entirely.
     * The state machine will move on to the next toolbar without sending
     * anything over USB — zero overhead for unchanged toolbars.
     */
    const uint32_t care_mask = UI_UPDATE_VOLTAGES | UI_UPDATE_LABELS | UI_UPDATE_FORCE;
    if (!(update_flags & care_mask)) {
        return 0;
    }

    /* Determine if this is a full paint (labels + states) or just states */
    bool full_paint = (update_flags & (UI_UPDATE_LABELS | UI_UPDATE_FORCE)) != 0;

    /*
     * Row 1: pin labels (only on full paint — labels are static)
     *
     * Position cursor at start_row, then render each pin label with its
     * per-pin foreground/background colors.  Pad remaining columns with
     * spaces to overwrite any stale content from a previous wider render.
     */
    if (full_paint) {
        len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row, 0);
        uint32_t cols = 0;
        for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
            len += ui_term_color_text_background_buf(&buf[len], buf_len - len,
                        hw_pin_label_ordered_color[i + 1][0],
                        hw_pin_label_ordered_color[i + 1][1]);
            int n = snprintf(&buf[len], buf_len - len, " %-4s",
                             hw_pin_label_ordered[i + 1]);
            len += n; cols += n;
        }
        /* Pad remaining columns to overwrite stale content */
        for (uint16_t c = cols; c < width; c++) {
            if (len < buf_len - 1) buf[len++] = ' ';
        }
        len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());
    }

    /*
     * Row 2: live pin states
     *
     * Always rendered when we pass the care_mask gate above.
     * Each pin gets a HIGH (red background) or LOW (black background) indicator.
     * Column padding at the end overwrites any stale content — this is the
     * key technique that avoids erase_line flicker.
     */
    len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row + 1, 0);

    uint32_t cols = 0;
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        bool high = bio_get(i);
        uint32_t fg = BP_COLOR_WHITE;
        uint32_t bg = high ? BP_COLOR_RED : BP_COLOR_FULLBLACK;
        len += ui_term_color_text_background_buf(&buf[len], buf_len - len, fg, bg);
        int n = snprintf(&buf[len], buf_len - len, " %-4s", high ? "HIGH" : "LOW");
        len += n; cols += n;
    }

    /* Pad remaining columns to overwrite stale content without erase */
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len,
                                             BP_COLOR_WHITE, BP_COLOR_FULLBLACK);
    for (uint16_t c = cols; c < width; c++) {
        if (len < buf_len - 1) buf[len++] = ' ';
    }

    /* Always reset colors at the end so the terminal returns to normal */
    len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());

    return len;
}

