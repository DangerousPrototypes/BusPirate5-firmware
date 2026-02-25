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
 * @brief Enable, register, and apply scroll region in one call.
 * @param tb  Toolbar to activate.
 * @return true on success, false if the registry is full.
 */
bool toolbar_activate(toolbar_t* tb);

/**
 * @brief Erase, unregister, disable, and restore scroll region.
 * @details Calls toolbar_draw_prepare(), toolbar_erase(), toolbar_unregister(),
 *          sets tb->enabled = false, toolbar_apply_scroll_region(), and
 *          toolbar_draw_release().  Safe to call even if the toolbar is not
 *          currently registered (no-op in that case).
 * @param tb  Toolbar to tear down.
 */
void toolbar_teardown(toolbar_t* tb);

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
 * @brief Erase a toolbar's screen area (save/restore cursor, clear each row).
 * @details Must be called while the toolbar is still registered so
 *          toolbar_get_start_row() can locate it.  Uses printf path (Core0 only).
 * @param tb  Toolbar whose rows should be erased.
 */
void toolbar_erase(const toolbar_t* tb);

/**
 * @brief Redraw all enabled toolbars (full repaint).
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
