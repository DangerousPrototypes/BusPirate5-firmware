/**
 * @file ui_mem_gui.h
 * @brief Reusable fullscreen GUI template for memory operations.
 *
 * Provides a complete "configure → execute → view results" workflow for
 * any command that reads/writes/dumps/verifies a memory device (EEPROM,
 * Flash, etc.).  The application provides field definitions, an execute
 * callback, and optionally a pre-check — the template handles everything
 * else: alt-screen lifecycle, menu bar, config bar, hex editor embedding,
 * progress/message rendering, and content area management.
 *
 * Usage:
 * @code
 *   static const ui_field_def_t my_fields[] = { ... };
 *
 *   ui_mem_gui_config_t config = {
 *       .title    = "I2C EEPROM",
 *       .fields   = my_fields,
 *       .field_count = ARRAY_SIZE(my_fields),
 *       .execute  = my_execute_fn,
 *       .ctx      = &my_app_state,
 *       .enable_hex_editor = true,
 *   };
 *   ui_mem_gui_run(&config);
 * @endcode
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef UI_MEM_GUI_H
#define UI_MEM_GUI_H

#include <stdint.h>
#include <stdbool.h>
#include "ui/ui_config_bar.h"
#include "lib/vt100_menu/vt100_menu.h"

/* ── Result message helpers ─────────────────────────────────────────── */

/** Maximum number of extra menus the application can provide. */
#define UI_MEM_GUI_MAX_EXTRA_MENUS 5

/* ── Operation status feedback ──────────────────────────────────────── */

/**
 * @brief Operation-area rendering callbacks.
 *
 * The template provides these for the application's execute function to
 * display progress, messages, errors, and warnings in the content area
 * (rows 4–7) while staying in the alt-screen.
 *
 * The application's execute function receives a pointer to this struct.
 */
typedef struct {
    void (*progress)(uint32_t current, uint32_t total, void *ctx);
    void (*message)(const char *msg, void *ctx);
    void (*error)(const char *msg, void *ctx);
    void (*warning)(const char *msg, void *ctx);
    void (*clear)(void *ctx);   /**< Clear content rows */
    void *ctx;                  /**< Internal — set by template */
} ui_mem_gui_ops_t;

/* ── Application config ─────────────────────────────────────────────── */

/**
 * @brief Configuration for a memory-operation GUI.
 *
 * The application fills this in and passes it to ui_mem_gui_run().
 */
typedef struct {
    const char *title;               /**< Title shown on the menu bar (e.g. "I2C EEPROM") */

    /* Config bar fields */
    const ui_field_def_t *fields;    /**< Field definition array */
    uint8_t               field_count;

    /* Extra menus — appended to the Options menu on the menu bar.
     * Set to NULL if not needed. */
    const vt100_menu_def_t *extra_menus;
    uint8_t                 extra_menu_count;

    /**
     * @brief Execute callback — the application's operation runner.
     *
     * Called when the user activates the Execute button or menu item.
     * The application should:
     *   1. Use ops->message() to show status text
     *   2. Use ops->progress() to update the progress bar
     *   3. Use ops->error()/warning() for problems
     *   4. Set *result_msg to a summary string on completion
     *
     * @param ctx         Application context
     * @param ops         Rendering callbacks for the content area
     * @param result_msg  Output: set to a static string summarising the result
     * @return true on success, false on failure
     */
    bool (*execute)(void *ctx, const ui_mem_gui_ops_t *ops, const char **result_msg);

    /**
     * @brief Optional: confirm destructive operations.
     *
     * If non-NULL, called before execute() for destructive actions.
     * Return true to proceed, false to abort.  If NULL, all operations
     * proceed without confirmation.
     */
    bool (*needs_confirm)(void *ctx);

    /**
     * @brief Optional: pre-flight check before execute().
     *
     * Return true if OK to proceed.  On false, set *err_msg to an
     * explanatory string.  If NULL, no pre-check is performed.
     */
    bool (*pre_check)(void *ctx, const char **err_msg);

    /**
     * @brief Optional: check if configuration is complete.
     *
     * Return true when all required fields are filled in and the
     * Execute button should be enabled.  If NULL, always enabled.
     */
    bool (*config_ready)(void *ctx);

    /**
     * @brief Optional: post-execute hook for loading results into hex editor.
     *
     * Called after a successful execute() when hex editor is enabled.
     * The application can call hx_embed_load() / hx_embed_load_buffer()
     * here.
     */
    void (*post_execute_load)(void *ctx);

    /**
     * @brief Optional: dispatch handler for extra menu action IDs.
     *
     * When the user selects an item from an extra menu, its action_id
     * is passed here.  The application should update its state (e.g.
     * set a spinner value via set_int).  If NULL, extra menu actions
     * are silently ignored.
     */
    void (*menu_dispatch)(void *ctx, int action_id);

    /** Optional: extra items to prepend in the Options menu.
     *  Useful for per-application config actions (e.g. "I2C Address").
     *  Set to NULL if not needed. */
    const vt100_menu_item_t *option_items;
    uint8_t                  option_item_count;

    /** Enable embedded hex editor (content rows 4+ when no operation running). */
    bool enable_hex_editor;

    /** Application context passed to all callbacks and field accessors. */
    void *ctx;
} ui_mem_gui_config_t;

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief Run the memory-operation GUI.
 *
 * Blocks until the user quits (Ctrl-Q, Esc, or Options→Quit).
 *
 * @param config  Application-provided configuration.
 * @return true if any operation was executed, false if user quit without executing.
 */
bool ui_mem_gui_run(const ui_mem_gui_config_t *config);

/**
 * @brief Request the framework to fire the execute callback.
 *
 * Call this from a BUTTON field's on_activate callback to trigger
 * the execute path (pre_check → confirm → execute) on the next
 * main-loop iteration.
 */
void ui_mem_gui_request_execute(void);

/**
 * @brief Open a file picker within the framework's I/O context.
 *
 * Callable from menu_dispatch or other app callbacks while the
 * framework is running.
 *
 * @param buf       Output buffer for the selected filename.
 * @param buf_size  Size of buf (at least 13 for 8.3 names).
 * @return true if a file was selected.
 */
bool ui_mem_gui_browse_file(char *buf, uint8_t buf_size);

/**
 * @brief Open a number input popup within the framework's I/O context.
 *
 * @param title   Popup title.
 * @param current Current value (pre-filled).
 * @param min     Minimum allowed value.
 * @param max     Maximum allowed value.
 * @param result  Output: the entered value.
 * @return true if the user confirmed a value.
 */
bool ui_mem_gui_popup_number(const char *title, uint32_t current,
                             uint32_t min, uint32_t max, uint32_t *result);

#endif /* UI_MEM_GUI_H */
