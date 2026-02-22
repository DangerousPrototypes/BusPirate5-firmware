/**
 * @file ui_toolbar.c
 * @brief Central toolbar registry and layout manager implementation.
 */

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

uint16_t toolbar_total_height(void) {
    uint16_t total = 0;
    for (uint8_t i = 0; i < toolbar_count; i++) {
        if (toolbar_registry[i]->enabled) {
            total += toolbar_registry[i]->height;
        }
    }
    return total;
}

uint16_t toolbar_scroll_top(void) {
    return 1;
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

    // Walk the registry from the bottom (last registered = bottommost)
    for (int8_t i = (int8_t)toolbar_count - 1; i >= 0; i--) {
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
    ui_term_scroll_region_printf(1, bottom);
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
