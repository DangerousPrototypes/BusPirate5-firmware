/**
 * @file vt100_menu.c
 * @brief VT100 dropdown menu system — implementation.
 * @details Renders a Norton Commander-style menu bar and handles F1-F5
 *          function keys, arrow navigation, Enter and ESC.
 *
 * Copyright (C) 2024 Ian Lesnet
 * SPDX-License-Identifier: MIT
 *
 * Modified by DangerousPrototypes — Bus Pirate integration (2024)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "vt100_menu.h"
#include "usb_rx.h" /* rx_fifo_try_get / rx_fifo_get_blocking */

/* -------------------------------------------------------------------------
 * Colour / style escape sequences
 * ------------------------------------------------------------------------- */

/** Menu bar: white text on blue background (like classic DOS menus). */
#define MENU_BAR_NORMAL    "\033[0;37;44m"
/** Menu bar: highlighted entry (black text on white). */
#define MENU_BAR_SELECTED  "\033[0;30;47m"
/** Dropdown body: white text on blue background. */
#define MENU_DROP_NORMAL   "\033[0;37;44m"
/** Dropdown: highlighted item (black text on white). */
#define MENU_DROP_SELECTED "\033[0;30;47m"
/** Reset all attributes. */
#define MENU_RESET         "\033[0m"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Position cursor at (row, col) — both 1-based.
 */
static void cursor_goto(uint16_t row, uint16_t col) {
    printf("\033[%d;%dH", row, col);
}

/**
 * @brief Save terminal cursor position. */
static void cursor_save(void) {
    printf("\0337");
}

/**
 * @brief Restore terminal cursor position. */
static void cursor_restore(void) {
    printf("\0338");
}

/**
 * @brief Read one character from the USB RX FIFO, blocking until available.
 */
static char menu_read_char(void) {
    char c;
    rx_fifo_get_blocking(&c);
    return c;
}

/* -------------------------------------------------------------------------
 * Menu bar rendering
 * ------------------------------------------------------------------------- */

/**
 * @brief Draw the menu bar at bar_row.
 *
 * Format:  [F1]Title1  [F2]Title2  ...
 */
void vt100_menu_draw_bar(const vt100_dropdown_t *dropdowns, uint8_t count,
                         uint16_t bar_row) {
    if (!dropdowns || count == 0) {
        return;
    }
    if (count > VT100_MENU_MAX_MENUS) {
        count = VT100_MENU_MAX_MENUS;
    }

    cursor_save();
    cursor_goto(bar_row, 1);
    printf("%s", MENU_BAR_NORMAL);

    for (uint8_t i = 0; i < count; i++) {
        /* Function key label */
        printf(" F%d:", i + 1);
        /* Menu title */
        printf("%s ", dropdowns[i].title ? dropdowns[i].title : "");
    }

    /* Erase to end of line to fill bar with background colour. */
    printf("\033[K");
    printf("%s", MENU_RESET);
    cursor_restore();
}

/* -------------------------------------------------------------------------
 * Dropdown rendering
 * ------------------------------------------------------------------------- */

/**
 * @brief Calculate the column at which menu_index starts in the bar.
 *
 * Each entry is " F<n>:<title> " wide.
 */
static uint16_t menu_bar_col(const vt100_dropdown_t *dropdowns, uint8_t count,
                              uint8_t menu_index) {
    uint16_t col = 1;
    for (uint8_t i = 0; i < menu_index && i < count; i++) {
        /* " F<n>:<title> " */
        col += 5 /* " F1:" */ + (uint16_t)strlen(dropdowns[i].title ? dropdowns[i].title : "") + 1 /* space */;
    }
    return col;
}

/**
 * @brief Draw the dropdown box for menu_index with cursor_item highlighted.
 */
static void draw_dropdown(const vt100_dropdown_t *dropdowns, uint8_t count,
                          uint8_t menu_index, uint8_t cursor_item,
                          uint16_t bar_row) {
    const vt100_dropdown_t *menu = &dropdowns[menu_index];
    uint16_t col = menu_bar_col(dropdowns, count, menu_index);
    uint16_t row = bar_row + 1;

    /* Calculate width: at least as wide as the widest item label. */
    uint16_t width = 10;
    for (uint8_t i = 0; i < menu->item_count; i++) {
        uint16_t llen = (uint16_t)strlen(menu->items[i].label ? menu->items[i].label : "");
        if (llen + 2 > width) {
            width = llen + 2;
        }
    }

    cursor_save();
    for (uint8_t i = 0; i < menu->item_count; i++) {
        cursor_goto(row + i, col);
        if (i == cursor_item) {
            printf("%s", MENU_DROP_SELECTED);
        } else {
            printf("%s", MENU_DROP_NORMAL);
        }
        const char *label = menu->items[i].label ? menu->items[i].label : "";
        uint16_t llen = (uint16_t)strlen(label);
        printf(" %-*s ", width - 2, label);
        (void)llen;
        printf("%s", MENU_RESET);
    }
    cursor_restore();
}

/**
 * @brief Erase the dropdown area (fill with spaces at normal colour).
 */
static void erase_dropdown(const vt100_dropdown_t *dropdowns, uint8_t count,
                            uint8_t menu_index, uint16_t bar_row) {
    const vt100_dropdown_t *menu = &dropdowns[menu_index];
    uint16_t col = menu_bar_col(dropdowns, count, menu_index);
    uint16_t row = bar_row + 1;

    /* Calculate width to erase */
    uint16_t width = 10;
    for (uint8_t i = 0; i < menu->item_count; i++) {
        uint16_t llen = (uint16_t)strlen(menu->items[i].label ? menu->items[i].label : "");
        if (llen + 2 > width) {
            width = llen + 2;
        }
    }

    cursor_save();
    printf("%s", MENU_RESET);
    for (uint8_t i = 0; i < menu->item_count; i++) {
        cursor_goto(row + i, col);
        /* Erase the dropdown row by overwriting with spaces. */
        for (uint16_t j = 0; j < width; j++) {
            printf(" ");
        }
    }
    cursor_restore();
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int32_t vt100_menu_run(const vt100_dropdown_t *dropdowns, uint8_t count,
                       uint16_t bar_row, uint8_t menu_index) {
    if (!dropdowns || count == 0 || menu_index >= count) {
        return VT100_MENU_CANCEL;
    }
    if (count > VT100_MENU_MAX_MENUS) {
        count = VT100_MENU_MAX_MENUS;
    }

    const vt100_dropdown_t *menu = &dropdowns[menu_index];
    if (menu->item_count == 0) {
        return VT100_MENU_CANCEL;
    }

    uint8_t cursor = 0;

    /* Highlight the active menu title in the bar. */
    cursor_save();
    uint16_t col = menu_bar_col(dropdowns, count, menu_index);
    cursor_goto(bar_row, col);
    printf("%s F%d:%s %s", MENU_BAR_SELECTED, menu_index + 1,
           menu->title ? menu->title : "", MENU_BAR_NORMAL);
    cursor_restore();

    /* Draw the dropdown. */
    draw_dropdown(dropdowns, count, menu_index, cursor, bar_row);

    /* Input loop. */
    while (true) {
        char c = menu_read_char();

        if (c == '\r' || c == '\n') {
            /* Confirm selection. */
            erase_dropdown(dropdowns, count, menu_index, bar_row);
            /* Restore normal bar highlight. */
            vt100_menu_draw_bar(dropdowns, count, bar_row);
            return (int32_t)menu->items[cursor].value;

        } else if (c == '\033') {
            /* Escape sequence or bare ESC. */
            char seq0;
            if (!rx_fifo_try_get(&seq0)) {
                /* Bare ESC — cancel. */
                erase_dropdown(dropdowns, count, menu_index, bar_row);
                vt100_menu_draw_bar(dropdowns, count, bar_row);
                return VT100_MENU_CANCEL;
            }

            if (seq0 == '[') {
                char seq1;
                if (!rx_fifo_try_get(&seq1)) {
                    /* ESC [ alone — treat as cancel. */
                    erase_dropdown(dropdowns, count, menu_index, bar_row);
                    vt100_menu_draw_bar(dropdowns, count, bar_row);
                    return VT100_MENU_CANCEL;
                }

                if (seq1 == 'A') {
                    /* Up arrow. */
                    if (cursor > 0) {
                        cursor--;
                    } else {
                        cursor = menu->item_count - 1;
                    }
                    draw_dropdown(dropdowns, count, menu_index, cursor, bar_row);

                } else if (seq1 == 'B') {
                    /* Down arrow. */
                    if (cursor < menu->item_count - 1) {
                        cursor++;
                    } else {
                        cursor = 0;
                    }
                    draw_dropdown(dropdowns, count, menu_index, cursor, bar_row);

                } else if (seq1 == 'C') {
                    /* Right arrow — switch to the next menu. */
                    erase_dropdown(dropdowns, count, menu_index, bar_row);
                    uint8_t next = (menu_index + 1) % count;
                    return vt100_menu_run(dropdowns, count, bar_row, next);

                } else if (seq1 == 'D') {
                    /* Left arrow — switch to the previous menu. */
                    erase_dropdown(dropdowns, count, menu_index, bar_row);
                    uint8_t prev = (menu_index == 0) ? (count - 1) : (menu_index - 1);
                    return vt100_menu_run(dropdowns, count, bar_row, prev);

                } else if (seq1 >= '0' && seq1 <= '9') {
                    /* Extended sequence — try to consume digits and '~'. */
                    char extra;
                    while (rx_fifo_try_get(&extra) && extra != '~') {
                        /* discard */
                    }
                    /* Ignore unknown sequences. */
                }
            } else if (seq0 == 'O') {
                /* ESC O sequences (VT100 function keys). */
                char seq1;
                if (rx_fifo_try_get(&seq1)) {
                    /* F1-F4 pressed while dropdown is open: switch to that menu. */
                    int fkey = -1;
                    if (seq1 == 'P') fkey = 1;
                    else if (seq1 == 'Q') fkey = 2;
                    else if (seq1 == 'R') fkey = 3;
                    else if (seq1 == 'S') fkey = 4;

                    if (fkey > 0 && (uint8_t)(fkey - 1) < count) {
                        erase_dropdown(dropdowns, count, menu_index, bar_row);
                        return vt100_menu_run(dropdowns, count, bar_row, (uint8_t)(fkey - 1));
                    }
                }
            }

        } else {
            /* Check for F1-F5 entered as their xterm two-byte sequences
             * that slipped through as individual chars — handled above via ESC.
             * For other printable chars, ignore inside a dropdown. */
        }
    }
}
