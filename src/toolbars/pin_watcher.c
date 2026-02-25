/**
 * @file pin_watcher.c
 * @brief 2-line GPIO pin state watcher toolbar implementation.
 * @details Line 1: colored pin labels (IO0..IO7).
 *          Line 2: live HIGH/LOW indicators with matching colors.
 *          All rendering goes through the Core1 _buf() path — the .draw callback
 *          simply triggers a blocking Core1 update via toolbar_update_blocking().
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "ui/ui_flags.h"

#define PIN_WATCHER_HEIGHT 2

/* Forward declarations */
static uint32_t pin_watcher_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                            uint16_t start_row, uint16_t width,
                                            uint32_t update_flags);

/**
 * @brief .draw callback — delegates to Core1 via toolbar_update_blocking().
 */
static void pin_watcher_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    (void)tb; (void)start_row; (void)width;
    toolbar_update_blocking();
}

static toolbar_t pin_watcher_toolbar = {
    .name       = "pin_watcher",
    .height     = PIN_WATCHER_HEIGHT,
    .enabled    = false,
    .owner_data = NULL,
    .draw       = pin_watcher_draw_cb,
    .update     = NULL,
    .update_core1 = pin_watcher_update_core1_cb,
    .destroy    = NULL,
};

bool pin_watcher_start(void) {
    if (pin_watcher_toolbar.enabled) {
        return true; /* already active */
    }
    /* Push content up before shrinking scroll region */
    for (uint16_t i = 0; i < PIN_WATCHER_HEIGHT; i++) {
        printf("\r\n");
    }
    if (!toolbar_activate(&pin_watcher_toolbar)) {
        return false;
    }
    /* Reposition cursor within the new scroll region */
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    return true;
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

void pin_watcher_update(void) {
    if (!pin_watcher_toolbar.enabled) {
        return;
    }
    toolbar_update_blocking();
}

/**
 * @brief Core1 update callback — renders pin labels and states into buffer.
 * @details Single rendering path for both initial paint and periodic refresh.
 *          - Row 1 (labels): only on UI_UPDATE_ALL / UI_UPDATE_FORCE (labels don't change)
 *          - Row 2 (states): on UI_UPDATE_VOLTAGES / UI_UPDATE_LABELS / UI_UPDATE_FORCE
 *          Skips entirely when no relevant flags are set.
 *          Uses only snprintf + _buf() variants.  Cursor envelope is handled
 *          by the state machine in ui_toolbar.c.
 */
static uint32_t pin_watcher_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                            uint16_t start_row, uint16_t width,
                                            uint32_t update_flags) {
    (void)tb;
    uint32_t len = 0;

    /* Skip if nothing relevant changed */
    const uint32_t care_mask = UI_UPDATE_VOLTAGES | UI_UPDATE_LABELS | UI_UPDATE_FORCE;
    if (!(update_flags & care_mask)) {
        return 0;
    }

    bool full_paint = (update_flags & (UI_UPDATE_LABELS | UI_UPDATE_FORCE)) != 0;

    /* Row 1: pin labels (only on full paint — labels are static) */
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
        /* Pad remaining columns */
        for (uint16_t c = cols; c < width; c++) {
            if (len < buf_len - 1) buf[len++] = ' ';
        }
        len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());
    }

    /* Row 2: live pin states */
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

    len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());

    return len;
}

