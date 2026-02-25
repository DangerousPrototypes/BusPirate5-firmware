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

static toolbar_t* toolbar_registry[TOOLBAR_MAX_COUNT];
static uint8_t toolbar_count = 0;

bool toolbar_register(toolbar_t* tb) {
    if (toolbar_count >= TOOLBAR_MAX_COUNT) {
        return false;
    }
    toolbar_registry[toolbar_count++] = tb;
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
    return true;
}

void toolbar_teardown(toolbar_t* tb) {
    if (!tb->enabled) {
        return;
    }
    toolbar_draw_prepare();
    ui_term_cursor_save();
    toolbar_erase(tb);
    toolbar_unregister(tb);
    tb->enabled = false;
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

void toolbar_erase(const toolbar_t* tb) {
    if (!system_config.terminal_ansi_color) {
        return;
    }
    uint16_t row = toolbar_get_start_row(tb);
    if (row == 0) {
        return; // not registered or disabled
    }
    ui_term_cursor_save();
    for (uint16_t i = 0; i < tb->height; i++) {
        ui_term_cursor_position(row + i, 0);
        ui_term_erase_line();
    }
    ui_term_cursor_restore();
}

void toolbar_redraw_all(void) {
    uint16_t width = system_config.terminal_ansi_columns;
    for (uint8_t i = 0; i < toolbar_count; i++) {
        toolbar_t* tb = toolbar_registry[i];
        if (tb->enabled && tb->draw) {
            uint16_t start_row = toolbar_get_start_row(tb);
            tb->draw(tb, start_row, width);
        }
    }
}

void toolbar_draw_prepare(void) {
    system_config.terminal_toolbar_pause = true;
    busy_wait_ms(1);
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
        printf("  [%u] \"%s\"  height=%u  enabled=%u  row=%u..%u  draw=%s\r\n",
               i, tb->name, tb->height, tb->enabled,
               start, start + tb->height - 1,
               tb->draw ? "yes" : "no");
    }
}
