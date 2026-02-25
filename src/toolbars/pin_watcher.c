/**
 * @file pin_watcher.c
 * @brief 2-line GPIO pin state watcher toolbar implementation.
 * @details Line 1: colored pin labels (IO0..IO7).
 *          Line 2: live HIGH/LOW indicators with matching colors.
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

#define PIN_WATCHER_HEIGHT 2

/* Forward declarations */
static void pin_watcher_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width);

static toolbar_t pin_watcher_toolbar = {
    .name       = "pin_watcher",
    .height     = PIN_WATCHER_HEIGHT,
    .enabled    = false,
    .owner_data = NULL,
    .draw       = pin_watcher_draw_cb,
    .update     = NULL,
    .destroy    = NULL,
};

/**
 * @brief Draw a single pin state row (line 2) at the given terminal row.
 */
static void pin_watcher_draw_states(uint16_t row) {
    ui_term_cursor_position(row, 0);
    ui_term_erase_line();

    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        bool high = bio_get(i);
        uint32_t fg = BP_COLOR_WHITE; //hw_pin_label_ordered_color[i + 1][0];
        uint32_t bg = high ? BP_COLOR_RED : BP_COLOR_FULLBLACK;
        ui_term_color_text_background(fg, bg);
        printf(" %-4s", high ? "HIGH" : "LOW");
    }
    printf("%s", ui_term_color_reset());
}

/**
 * @brief .draw callback — paints pin labels (row 1) and states (row 2).
 */
static void pin_watcher_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    (void)tb; (void)width;

    toolbar_draw_prepare();
    ui_term_cursor_save();

    /* Row 1: pin labels with their assigned colors */
    ui_term_cursor_position(start_row, 0);
    ui_term_erase_line();
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        ui_term_color_text_background(
            hw_pin_label_ordered_color[i + 1][0],
            hw_pin_label_ordered_color[i + 1][1]);
        printf(" %-4s", hw_pin_label_ordered[i + 1]);
    }
    printf("%s", ui_term_color_reset());

    /* Row 2: live pin states */
    pin_watcher_draw_states(start_row + 1);

    ui_term_cursor_restore();
    toolbar_draw_release();
}

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
    pin_watcher_draw_cb(&pin_watcher_toolbar,
                        toolbar_get_start_row(&pin_watcher_toolbar),
                        system_config.terminal_ansi_columns);
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
    uint16_t start_row = toolbar_get_start_row(&pin_watcher_toolbar);
    if (start_row == 0) {
        return;
    }
    toolbar_draw_prepare();
    ui_term_cursor_save();
    pin_watcher_draw_states(start_row + 1);
    ui_term_cursor_restore();
    toolbar_draw_release();
}

