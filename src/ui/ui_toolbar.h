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

/**
 * @brief Toolbar descriptor.
 * @details Filled in by the toolbar owner and passed to toolbar_register().
 */
typedef struct toolbar_t {
    const char* name;     ///< Unique name string (e.g. "statusbar", "logic_analyzer")
    uint16_t height;      ///< Number of terminal lines this toolbar occupies
    bool enabled;         ///< Currently active / visible
    bool anchor_bottom;   ///< Always insert at bottom of stack (index 0)
    void* owner_data;     ///< Opaque pointer for the toolbar owner's private state

    /**
     * @brief Full redraw of this toolbar.
     * @param start_row  First row this toolbar occupies (1-based).
     * @param width      Terminal column width.
     */
    void (*draw)(struct toolbar_t* tb, uint16_t start_row, uint16_t width);

    /**
     * @brief Partial update of this toolbar.
     * @param start_row   First row this toolbar occupies (1-based).
     * @param width       Terminal column width.
     * @param update_flags Bitmask of sections that changed (toolbar-specific meaning).
     */
    void (*update)(struct toolbar_t* tb, uint16_t start_row, uint16_t width, uint32_t update_flags);

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
    uint32_t (*update_core1)(struct toolbar_t* tb, char* buf, size_t buf_len,
                             uint16_t start_row, uint16_t width, uint32_t update_flags);

    /**
     * @brief Called when the toolbar is unregistered; release any resources.
     */
    void (*destroy)(struct toolbar_t* tb);
} toolbar_t;

/**
 * @brief Register a toolbar in the registry.
 * @param tb  Pointer to caller-owned toolbar_t (must remain valid until unregistered).
 * @return true on success, false if the registry is full.
 */
bool toolbar_register(toolbar_t* tb);

/**
 * @brief Unregister a toolbar and recalculate layout.
 * @param tb  Toolbar to remove.
 */
void toolbar_unregister(toolbar_t* tb);

/**
 * @brief Enable, register, apply scroll region, and redraw all toolbars.
 * @details After registering the toolbar and applying the new scroll region,
 *          calls toolbar_redraw_all() so every toolbar (including the new one)
 *          is painted at its correct position.  Callers only need to push
 *          blank lines beforehand and reposition the cursor afterward.
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
