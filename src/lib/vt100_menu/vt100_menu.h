/**
 * @file vt100_menu.h
 * @brief VT100 dropdown menu system for Bus Pirate command GUIs.
 * @details Provides a Norton Commander-style menu bar with dropdown menus
 *          triggered by F1-F5 function keys.  Each function key directly
 *          opens its corresponding menu (F1 = first menu in order of
 *          appearance, F2 = second, etc., up to five menus maximum).
 *
 * Copyright (C) 2024 Ian Lesnet
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Maximum number of dropdown menus (one per function key F1-F5). */
#define VT100_MENU_MAX_MENUS   5

/* Maximum number of selectable items in a single dropdown. */
#define VT100_MENU_MAX_ITEMS   16

/* Sentinel value: returned by vt100_menu_run() when the user pressed ESC. */
#define VT100_MENU_CANCEL      ((int32_t)-1)

/**
 * @brief A single selectable entry in a dropdown menu.
 */
typedef struct {
    const char *label;  /**< Text displayed for this option.           */
    uint32_t    value;  /**< Caller-defined value returned on select.  */
} vt100_menu_item_t;

/**
 * @brief One dropdown menu with its title and list of items.
 */
typedef struct {
    const char              *title;      /**< Title shown in the menu bar.  */
    const vt100_menu_item_t *items;      /**< Array of selectable items.    */
    uint8_t                  item_count; /**< Length of items[].            */
} vt100_dropdown_t;

/**
 * @brief Draw the menu bar at a fixed terminal row.
 *
 * Should be called once after the command GUI has set up its screen area.
 * The bar is rendered at the specified row using the style:
 *   white text on blue background, function key labels preceding each title.
 *
 * @param dropdowns   Array of dropdown definitions (up to VT100_MENU_MAX_MENUS).
 * @param count       Number of entries in dropdowns[].
 * @param bar_row     Terminal row (1-based) for the menu bar.
 */
void vt100_menu_draw_bar(const vt100_dropdown_t *dropdowns, uint8_t count,
                         uint16_t bar_row);

/**
 * @brief Open a dropdown by index and let the user choose an item.
 *
 * Draws the dropdown below the menu bar, handles arrow-key navigation,
 * Enter to confirm, and ESC to cancel.  Returns immediately after the
 * user makes a choice or cancels.
 *
 * @param dropdowns    Same array that was passed to vt100_menu_draw_bar().
 * @param count        Length of dropdowns[].
 * @param bar_row      Terminal row of the menu bar (same as draw_bar call).
 * @param menu_index   Which dropdown to open (0 = first, 1 = second, …).
 * @return             Selected item's value, or VT100_MENU_CANCEL.
 */
int32_t vt100_menu_run(const vt100_dropdown_t *dropdowns, uint8_t count,
                       uint16_t bar_row, uint8_t menu_index);
