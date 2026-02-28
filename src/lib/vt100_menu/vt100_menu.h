/*
 * vt100_menu.h — VT100 top menu bar with dropdown selection framework
 *
 * Provides a classic TUI menu bar (à la Turbo Pascal / Norton Commander)
 * that works on any VT100-compatible terminal.  Designed for Bus Pirate
 * fullscreen apps (edit, hexedit, scope, etc.) where users currently
 * have to memorize hidden Ctrl-key combos.
 *
 * Layout:
 *   Row 1:  ┌─ File ─┬─ Edit ─┬─ Search ─┬─ Help ─────────────────┐
 *   Row 2+: │ content area (editor, hex view, etc.)                 │
 *
 * When a menu is "open", a dropdown box overlays the content:
 *   Row 1:  ┌─[File]─┬─ Edit ─┬─ Search ─┬─ Help ─────────────────┐
 *   Row 2:  │┌──────────┐                                          │
 *   Row 3:  ││ Save   S │                                          │
 *   Row 4:  ││ Save As  │                                          │
 *   Row 5:  ││──────────│                                          │
 *   Row 6:  ││ Quit   Q │                                          │
 *   Row 7:  │└──────────┘                                          │
 *
 * Activation:
 *   - F10 or Alt key opens the menu bar (configurable)
 *   - Left/Right arrows move between menus
 *   - Enter selects an item
 *   - Escape closes the menu
 *
 * Integration:
 *   1. Define your menus as static arrays of vt100_menu_item_t
 *   2. Define top-level menus as vt100_menu_def_t array
 *   3. Call vt100_menu_init() with your menu definition
 *   4. In your key handler, call vt100_menu_process_key() when the
 *      menu activation key (F10/Alt) is detected
 *   5. The framework returns an action ID for the selected command
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef VT100_MENU_H
#define VT100_MENU_H

#include <stdint.h>
#include <stdbool.h>

/* ── Menu item flags ────────────────────────────────────────────────── */

#define MENU_ITEM_SEPARATOR  0x01  /**< Horizontal line divider, not selectable */
#define MENU_ITEM_DISABLED   0x02  /**< Greyed out, not selectable */
#define MENU_ITEM_CHECKED    0x04  /**< Show a checkmark beside the label */

/* ── Return codes from vt100_menu_process_key() ─────────────────────── */

#define MENU_RESULT_NONE        0   /**< Menu still open, no selection yet */
#define MENU_RESULT_CANCEL    -1   /**< User pressed Escape or clicked outside */
#define MENU_RESULT_REDRAW    -2   /**< Menu closed, caller should redraw screen */
#define MENU_RESULT_PASSTHROUGH -3 /**< Menu closed, unhandled key in state->unhandled_key */

/* ── Data structures ────────────────────────────────────────────────── */

/**
 * @brief A single item within a dropdown menu.
 *
 * Terminate the array with a sentinel: { NULL, 0, 0, 0 }
 */
typedef struct {
    const char* label;       /**< Display text (NULL = end sentinel) */
    const char* shortcut;    /**< Right-aligned shortcut hint (e.g. "^S"), or NULL */
    int         action_id;   /**< Unique action ID returned on selection (>0) */
    uint8_t     flags;       /**< MENU_ITEM_* flags */
} vt100_menu_item_t;

/**
 * @brief A top-level menu (one tab on the menu bar).
 *
 * Terminate the array with a sentinel: { NULL, NULL, 0 }
 */
typedef struct {
    const char*              label;  /**< Menu bar tab label (NULL = end sentinel) */
    const vt100_menu_item_t* items;  /**< Pointer to array of dropdown items */
    uint8_t                  count;  /**< Number of items (excluding sentinel) */
} vt100_menu_def_t;

/**
 * @brief Runtime state of the menu system.
 *
 * Allocate one of these (statically or in arena) and pass it to all
 * vt100_menu_* functions.  The struct is small (~24 bytes) and does
 * not allocate any dynamic memory.
 */
typedef struct {
    const vt100_menu_def_t* menus;      /**< Pointer to menu definitions array */
    uint8_t  menu_count;                /**< Number of top-level menus */
    bool     active;                    /**< Menu bar is currently displayed/open */
    bool     dropdown_open;             /**< A dropdown is currently visible */
    uint8_t  selected_menu;             /**< Index of highlighted top-level menu */
    int8_t   selected_item;             /**< Index of highlighted dropdown item (-1 = none) */
    uint8_t  bar_row;                   /**< Terminal row for the menu bar (usually 1) */
    uint8_t  screen_cols;               /**< Terminal width for rendering */
    uint8_t  screen_rows;               /**< Terminal height */

    /* Callback: read one key from the terminal.
     * Must behave like hx's read_key() — return key code or virtual key enum.
     * This decouples the menu from any specific editor's I/O layer. */
    int (*read_key)(void);

    /* Callback: write raw bytes to the terminal.
     * Signature matches write(fd, buf, count).  fd is ignored. */
    int (*write_out)(int fd, const void* buf, int count);

    /* Callback: repaint the editor screen content (optional).
     * Called when switching between dropdown menus so the area under
     * the old dropdown is properly restored instead of left blank.
     * If NULL, erase_dropdown() blanks are used as a fallback. */
    void (*repaint)(void);

    /* Key codes — set by vt100_menu_init() from compile-time defaults,
     * but callers can override after init for editors with different enums.
     * These MUST live in the state because vt100_menu.c is compiled as its
     * own translation unit and cannot see the caller's #define overrides. */
    int key_up;
    int key_down;
    int key_left;
    int key_right;
    int key_enter;
    int key_esc;
    int key_f10;

    /* After vt100_menu_run() returns MENU_RESULT_PASSTHROUGH, this holds
     * the key code that the menu did not consume.  The caller should
     * push it back into its key-read function for normal processing. */
    int unhandled_key;
} vt100_menu_state_t;

/* ── Box-drawing style ──────────────────────────────────────────────── */

/**
 * @brief Box drawing character set.
 *
 * Uses plain ASCII by default for maximum terminal compatibility.
 * Could be switched to Unicode box-drawing if the terminal supports it.
 */
typedef struct {
    char tl;  /**< Top-left corner     */
    char tr;  /**< Top-right corner    */
    char bl;  /**< Bottom-left corner  */
    char br;  /**< Bottom-right corner */
    char h;   /**< Horizontal line     */
    char v;   /**< Vertical line       */
    char lt;  /**< Left T-junction     */
    char rt;  /**< Right T-junction    */
} vt100_menu_box_t;

/* Default ASCII box characters */
#define VT100_MENU_BOX_ASCII  { '+', '+', '+', '+', '-', '|', '+', '+' }

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief Initialise the menu state.
 *
 * @param state       Menu state struct to initialise.
 * @param menus       Array of top-level menu definitions (NULL-terminated).
 * @param menu_count  Number of top-level menus.
 * @param bar_row     Terminal row for the menu bar (1 = top of screen).
 * @param cols        Terminal width in columns.
 * @param rows        Terminal height in rows.
 * @param read_key_fn Callback to read a keypress.
 * @param write_fn    Callback to write terminal output.
 */
void vt100_menu_init(vt100_menu_state_t* state,
                     const vt100_menu_def_t* menus,
                     uint8_t menu_count,
                     uint8_t bar_row,
                     uint8_t cols,
                     uint8_t rows,
                     int (*read_key_fn)(void),
                     int (*write_fn)(int, const void*, int));

/**
 * @brief Open the menu bar and enter the interactive menu loop.
 *
 * Call this when the user presses F10 (or your chosen activation key).
 * The function enters a blocking loop, reading keys via read_key(),
 * until the user either selects an item or cancels.
 *
 * @param state  Menu state.
 * @return >0: action_id of the selected menu item.
 *         MENU_RESULT_CANCEL: user pressed Escape.
 *         MENU_RESULT_REDRAW: menu closed, screen needs repaint.
 */
int vt100_menu_run(vt100_menu_state_t* state);

/**
 * @brief Draw just the menu bar (row 1), without opening a dropdown.
 *
 * Useful for rendering the bar as part of the normal screen refresh
 * (e.g. showing "F10=Menu" hint) without entering interactive mode.
 *
 * @param state  Menu state.
 */
void vt100_menu_draw_bar(const vt100_menu_state_t* state);

/**
 * @brief Erase the menu bar row (restore it to blank).
 *
 * Call after vt100_menu_run() returns to clean up before the editor
 * redraws its own content.
 *
 * @param state  Menu state.
 */
void vt100_menu_erase(const vt100_menu_state_t* state);

/**
 * @brief Calculate how many rows the menu bar reserves.
 *
 * Returns 1 when the bar is shown (for the menu bar row itself).
 * Returns 0 when the menu system is not active.
 * Does NOT count dropdown overlay rows (those are temporary overlays).
 *
 * @param state  Menu state.
 * @return Number of reserved rows (0 or 1).
 */
uint8_t vt100_menu_reserved_rows(const vt100_menu_state_t* state);

/* ── Key constants (must match the host editor's key enum) ──────────── */
/*
 * The menu framework needs to recognise arrow keys, Enter, and Escape.
 * These defaults use vt100_keys.h virtual key codes.  If your editor uses
 * different values, #define VT100_MENU_KEY_* before including this header,
 * or override the key_* fields on the state struct after vt100_menu_init().
 */
#include "lib/vt100_keys/vt100_keys.h"
#ifndef VT100_MENU_KEY_UP
#define VT100_MENU_KEY_UP      VT100_KEY_UP
#endif
#ifndef VT100_MENU_KEY_DOWN
#define VT100_MENU_KEY_DOWN    VT100_KEY_DOWN
#endif
#ifndef VT100_MENU_KEY_RIGHT
#define VT100_MENU_KEY_RIGHT   VT100_KEY_RIGHT
#endif
#ifndef VT100_MENU_KEY_LEFT
#define VT100_MENU_KEY_LEFT    VT100_KEY_LEFT
#endif
#ifndef VT100_MENU_KEY_ENTER
#define VT100_MENU_KEY_ENTER   VT100_KEY_ENTER
#endif
#ifndef VT100_MENU_KEY_ESC
#define VT100_MENU_KEY_ESC     VT100_KEY_ESC
#endif
#ifndef VT100_MENU_KEY_F10
#define VT100_MENU_KEY_F10     VT100_KEY_F10
#endif

#endif /* VT100_MENU_H */
