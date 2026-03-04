/**
 * @file ui_config_bar.h
 * @brief Data-driven config bar — replaces hand-coded field rendering & key handling.
 *
 * Define fields as a const array of ui_field_def_t. The framework handles:
 *   - Rendering: bracketed widgets ([value v], [xVfy], [Execute]) with focus highlight
 *   - Key dispatch: Tab, Left/Right, Up/Down, Enter, Space per field type
 *   - Focus management: auto-skip hidden fields, wrap around
 *   - Widget integration: launches file picker, number popup, etc.
 *
 * All state access goes through get/set callbacks — no internal value storage.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef UI_CONFIG_BAR_H
#define UI_CONFIG_BAR_H

#include <stdint.h>
#include <stdbool.h>

/* ── Field types ────────────────────────────────────────────────────── */

typedef enum {
    UI_FIELD_SPINNER,   /**< Up/Down cycles through string options */
    UI_FIELD_CHECKBOX,  /**< Toggle with Space/Enter/Up/Down */
    UI_FIELD_NUMBER,    /**< Enter opens number popup, Up/Down ±1 */
    UI_FIELD_FILE,      /**< Enter opens file picker */
    UI_FIELD_BUTTON,    /**< Enter triggers on_activate callback */
} ui_field_type_t;

/* ── Field definition ───────────────────────────────────────────────── */

/**
 * @brief Describes a single config-bar field.
 *
 * Typically declared in a const array. The framework reads the definition
 * and calls the get/set/activate callbacks to interact with the app state.
 */
typedef struct ui_field_def {
    ui_field_type_t type;   /**< Widget type */
    uint8_t         width;  /**< Display width in columns (content, not including brackets) */

    /* Type-specific configuration — use the member matching .type */
    union {
        struct {
            const char *const *options;    /**< Display string array */
            uint8_t             count;     /**< Number of options */
            bool                wrap;      /**< Wrap at ends? */
        } spinner;
        struct {
            const char *text;              /**< Label text (e.g. "Vfy") */
        } checkbox;
        struct {
            uint32_t    min;               /**< Minimum value */
            uint32_t    max;               /**< Maximum value */
            const char *fmt;               /**< printf format (e.g. "0x%02X") */
            const char *popup_title;       /**< Title for the number popup */
        } number;
        struct {
            const char *ext_filter;        /**< Extension filter for picker (or NULL) */
        } file;
        struct {
            const char *text;              /**< Button label (e.g. "Execute") */
            bool (*ready)(void *ctx);      /**< Non-NULL: grey out when false */
        } button;
    };

    /* ── Value accessors (called by framework) ── */

    /** Get integer value: spinner index, checkbox bool, number value.
     *  Return -1 for "not set" on spinners. */
    int  (*get_int)(void *ctx);

    /** Set integer value. */
    void (*set_int)(void *ctx, int val);

    /** Get string value (file field). May return NULL or empty. */
    const char * (*get_str)(void *ctx);

    /** Set string value (file field). */
    void (*set_str)(void *ctx, const char *val);

    /** Activation callback (button press, Enter on field). May be NULL. */
    void (*on_activate)(void *ctx);

    /** Visibility predicate. NULL = always visible.
     *  When false, the field is hidden and skipped during focus navigation. */
    bool (*visible)(void *ctx);
} ui_field_def_t;

/* ── Config bar state ───────────────────────────────────────────────── */

typedef struct {
    const ui_field_def_t *fields;     /**< Field definition array */
    uint8_t               field_count;/**< Number of fields */
    uint8_t               focused;    /**< Currently focused field index */
    void                 *ctx;        /**< App context passed to all callbacks */
    uint8_t               bar_row;    /**< Terminal row to draw on (1-based) */
    uint8_t               cols;       /**< Terminal width */
    uint8_t               rows;       /**< Terminal height (for popups) */

    /** Write callback for rendering. Matches vt100_menu signature. */
    int (*write_out)(int fd, const void *buf, int count);

    /** Read-key callback for popup interactions (file picker, number input). */
    int (*read_key)(void);

    /** Optional repaint callback — called after a popup closes to restore
     *  the screen behind the overlay. */
    void (*repaint)(void);
} ui_config_bar_t;

/* ── API ────────────────────────────────────────────────────────────── */

/**
 * @brief Initialise a config bar.
 */
void ui_config_bar_init(ui_config_bar_t *bar,
                        const ui_field_def_t *fields,
                        uint8_t count,
                        void *ctx,
                        uint8_t row,
                        uint8_t cols,
                        uint8_t rows);

/**
 * @brief Render the config bar at its assigned row.
 *
 * Draws all visible fields as bracketed widgets with appropriate
 * focus highlighting. Call once per frame.
 */
void ui_config_bar_draw(ui_config_bar_t *bar);

/**
 * @brief Process a key event.
 *
 * Handles Tab (focus next), Shift-Tab/Left (focus prev), Up/Down (value change),
 * Enter (activate), Space (toggle). Returns true if the key was consumed.
 *
 * @return true if the key was handled, false if the caller should process it
 */
bool ui_config_bar_handle_key(ui_config_bar_t *bar, int key);

/**
 * @brief Move focus to the next visible field (wraps around).
 */
void ui_config_bar_focus_next(ui_config_bar_t *bar);

/**
 * @brief Move focus to the previous visible field (wraps around).
 */
void ui_config_bar_focus_prev(ui_config_bar_t *bar);

/**
 * @brief Get the index of the currently focused field.
 */
uint8_t ui_config_bar_focused(const ui_config_bar_t *bar);

#endif /* UI_CONFIG_BAR_H */
