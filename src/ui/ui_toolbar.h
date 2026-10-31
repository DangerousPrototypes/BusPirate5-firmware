/**
 * @file ui_toolbar.h
 * @brief Central toolbar registry and layout manager.
 * @details Provides a registry for bottom-of-screen toolbars that stack above
 *          the command scroll area.  Each toolbar registers its height and draw
 *          callbacks; the registry owns the scroll-region calculation so every
 *          toolbar always lands in the correct screen position.
 *
 *          Layout (bottom of terminal):
 *          @code
 *          ┌─────────────────────┐ ← row 1
 *          │ scrollable area     │
 *          ├─────────────────────┤ ← scroll_bottom = rows - total_height
 *          │ [toolbar 0]         │  e.g. logic_analyzer (10 lines)
 *          │ [toolbar 1]         │  e.g. statusbar      (4 lines)
 *          └─────────────────────┘ ← row = terminal_ansi_rows
 *          @endcode
 *
 *          The statusbar is always registered last (bottommost).
 *          New toolbars stack above it in registration order.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TOOLBAR_MAX_COUNT 4 ///< Maximum number of simultaneously registered toolbars

/* Forward declaration — toolbar_t is defined after toolbar_def_t */
typedef struct toolbar_t toolbar_t;

/**
 * @brief Immutable toolbar definition — lives in FLASH.
 * @details Contains all fields that never change after initialisation:
 *          name, default height, anchor flag, and all callback pointers.
 *          Each toolbar module declares one `static const toolbar_def_t`
 *          and points its mutable `toolbar_t.def` at it.
 */
typedef struct toolbar_def {
    const char* name;     ///< Unique name string (e.g. "statusbar", "logic_analyzer")
    uint16_t height;      ///< Default number of terminal lines this toolbar occupies
    bool anchor_bottom;   ///< Always insert at bottom of stack (index 0)

    /**
     * @brief Full redraw of this toolbar.
     * @param start_row  First row this toolbar occupies (1-based).
     * @param width      Terminal column width.
     */
    void (*draw)(toolbar_t* tb, uint16_t start_row, uint16_t width);

    /**
     * @brief Core1 periodic update — render into a buffer (no printf!).
     * @details Called from the Core1 toolbar state machine.  Must use only
     *          `_buf()` variants and snprintf.  Cursor save/hide and
     *          restore/show are handled by the caller.
     * @param tb           This toolbar.
     * @param buf          Buffer to write VT100 content into.
     * @param buf_len      Available bytes in @p buf.
     * @param start_row    First row (1-based).
     * @param width        Terminal width in columns.
     * @param update_flags UI_UPDATE_* bitmask from the monitor subsystem.
     * @return Number of bytes written, or 0 to skip this cycle.
     */
    uint32_t (*update_core1)(toolbar_t* tb, char* buf, size_t buf_len,
                             uint16_t start_row, uint16_t width, uint32_t update_flags);

    /**
     * @brief Called when the toolbar is unregistered; release any resources.
     */
    void (*destroy)(toolbar_t* tb);

    bool focusable;   ///< Supports TAB focus (false by default via zero-init)

    /**
     * @brief Handle a key while this toolbar has focus.
     * @param tb   This toolbar.
     * @param key  VT100_KEY_* code (arrow keys, page up/down, etc.).
     * @return true if the key was consumed, false to ignore.
     * @note Only called when focusable is true and tb->focused is set.
     */
    bool (*handle_key)(toolbar_t* tb, int key);
} toolbar_def_t;

/**
 * @brief Mutable toolbar runtime state — lives in RAM.
 * @details Only the fields that actually change at runtime are kept here.
 *          The immutable descriptor lives in FLASH via the `def` pointer.
 */
struct toolbar_t {
    const toolbar_def_t* def;  ///< Pointer to immutable FLASH descriptor
    uint16_t height;           ///< Runtime height (copied from def, can be overridden)
    bool enabled;              ///< Currently active / visible
    bool focused;              ///< Currently has TAB input focus
    void* owner_data;          ///< Opaque pointer for the toolbar owner's private state
};

/**
 * @brief Push blank lines, register, apply scroll region, redraw, and reposition cursor.
 * @details Pushes \\r\\n × height to scroll existing content up, registers the toolbar,
 *          applies the new scroll region, redraws all toolbars, and repositions the
 *          cursor at the bottom of the new scroll area.  After this returns true,
 *          the toolbar is fully visible and the cursor is in the correct position.
 * @param tb  Toolbar to activate.
 * @return true on success, false if the registry is full.
 */
bool toolbar_activate(toolbar_t* tb);

/**
 * @brief Erase all toolbar rows, unregister, restore scroll region, and redraw remaining.
 * @details Erases the entire toolbar area, unregisters @p tb, sets
 *          tb->enabled = false, applies the new scroll region, then redraws
 *          all remaining toolbars at their updated positions.  This ensures
 *          mid-stack removal never leaves stale rows.
 *          Safe to call even if the toolbar is not currently registered (no-op).
 * @param tb  Toolbar to tear down.
 */
void toolbar_teardown(toolbar_t* tb);

/**
 * @brief Tear down all registered toolbars and restore full-screen scroll.
 * @details Erases the entire toolbar area, tears down every toolbar in the
 *          registry (calling .destroy if set), resets the scroll region to
 *          full screen.  Used before reboot / bootloader jump.
 */
void toolbar_teardown_all(void);

/**
 * @brief Sum of all enabled toolbar heights (lines reserved at the bottom).
 * @return Total lines reserved.
 */
uint16_t toolbar_total_height(void);

/**
 * @brief Number of currently registered toolbars.
 * @return Count of registered toolbar slots in use.
 */
uint8_t toolbar_count_registered(void);

/**
 * @brief Last row of the scrollable command area.
 * @return terminal_ansi_rows - toolbar_total_height()
 */
uint16_t toolbar_scroll_bottom(void);

/**
 * @brief First terminal row used by a specific toolbar.
 * @param tb  Toolbar to query.
 * @return 1-based start row, or 0 if not registered / disabled.
 */
uint16_t toolbar_get_start_row(const toolbar_t* tb);

/**
 * @brief Apply the current scroll region to the terminal.
 * @details Emits \033[1;bottomr so the command area scrolls correctly.
 *          No-op when VT100 or statusbar is disabled.
 */
void toolbar_apply_scroll_region(void);

/**
 * @brief Redraw all enabled toolbars (full repaint).
 * @details Wraps the batch in toolbar_draw_prepare/release and cursor
 *          save/restore.  Individual .draw callbacks are pure painters.
 */
void toolbar_redraw_all(void);

/**
 * @brief Pause Core1 toolbar updates and hide the cursor.
 * @details Pauses the statusbar update loop, waits 1 ms for any in-flight
 *          render to drain, then hides the cursor.  Pairs with
 *          toolbar_draw_release().  Used by any Core0 code that needs to
 *          draw directly in toolbar screen rows (e.g. logic bar frame).
 */
void toolbar_draw_prepare(void);

/**
 * @brief Resume Core1 toolbar updates and restore the cursor.
 * @details Undoes toolbar_draw_prepare().
 */
void toolbar_draw_release(void);

/**
 * @brief Pause Core1 toolbar updates (no cursor change).
 * @details Used by subsystems (bridges, monitors, progress bars) that stream
 *          raw data and need to prevent VT100 escape interleaving.
 */
void toolbar_pause_updates(void);

/**
 * @brief Resume Core1 toolbar updates (no cursor change).
 * @details Undoes toolbar_pause_updates().
 */
void toolbar_resume_updates(void);

/**
 * @brief Print the toolbar registry contents to the terminal (debug/dev).
 */
void toolbar_print_registry(void);

/**
 * @brief Request a full toolbar update from Core0 (blocking).
 * @details Sends BP_ICM_UPDATE_TOOLBARS to Core1 which kicks the
 *          cooperative state machine with UI_UPDATE_ALL.  Core0 blocks
 *          until Core1 acknowledges receipt (not until rendering completes).
 */
void toolbar_update_blocking(void);

/**
 * @brief Begin a Core1 toolbar update cycle.
 * @details Called from the Core1 main loop when lcd_update_request fires.
 *          Starts the cooperative state machine that renders each toolbar
 *          with a `.update_core1` callback, one per service call.
 * @param update_flags  UI_UPDATE_* bitmask from the monitor subsystem.
 */
void toolbar_core1_begin_update(uint32_t update_flags);

/**
 * @brief Service one step of the Core1 toolbar state machine.
 * @details Call every iteration of the Core1 main loop (after tx_fifo_service).
 *          - IDLE: returns immediately.
 *          - RENDERING: renders the next toolbar into tx_tb_buf, starts drain.
 *          - DRAINING: checks if drain is complete, advances to next toolbar.
 */
void toolbar_core1_service(void);

/**
 * @brief Find the next focusable toolbar in the registry.
 * @param current  Current focused toolbar (NULL to find the first).
 * @return Next focusable toolbar, or NULL if none found.
 */
toolbar_t* toolbar_next_focusable(toolbar_t* current);

/* ── Focus state machine (called from Core0 main loop) ────────────── */

/**
 * @brief Result codes from toolbar_focus_handle_key().
 */
typedef enum {
    TB_FOCUS_CONTINUE,  ///< Key consumed (or ignored), stay in focus mode
    TB_FOCUS_EXIT,      ///< Focus ended — caller should return to prompt
} toolbar_focus_result_t;

/**
 * @brief Enter focus mode on the first focusable toolbar.
 * @details Finds the first focusable toolbar, sets its `.focused` flag,
 *          redraws all toolbars (cursor stays hidden via draw_release
 *          focus-awareness).
 * @return true if a focusable toolbar was found and focused.
 *         false if no focusable toolbar exists (caller should not enter focus state).
 */
bool toolbar_focus_enter(void);

/**
 * @brief Process one key while in focus mode.
 * @details Handles ESC/Ctrl+C (exit), TAB (cycle), and routes all other
 *          keys to the focused toolbar's `.handle_key` callback.
 * @param key  VT100_KEY_* code from vt100_key_read_rx_fifo().
 * @return TB_FOCUS_CONTINUE to stay in focus, TB_FOCUS_EXIT to return to prompt.
 */
toolbar_focus_result_t toolbar_focus_handle_key(int key);
