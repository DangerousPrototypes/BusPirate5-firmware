/**
 * @file ui_popup.h
 * @brief Reusable VT100 popup dialogs — confirm, text input, number input.
 *
 * Each popup draws a centered box overlay on the current screen, collects
 * input, and returns the result.  The caller's screen is NOT restored;
 * call your repaint function after the popup returns.
 *
 * I/O is routed through the caller's write callback so these work inside
 * any fullscreen app (embedded mode).
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef UI_POPUP_H
#define UI_POPUP_H

#include <stdint.h>
#include <stdbool.h>

/* ── Style constants ────────────────────────────────────────────────── */

/** Popup visual style — determines the border/background color palette. */
typedef enum {
    UI_POPUP_INFO   = 0, /**< White on blue  (neutral/input) */
    UI_POPUP_WARN   = 1, /**< White on yellow (warning) */
    UI_POPUP_DANGER = 2, /**< White on red    (destructive confirm) */
} ui_popup_style_t;

/** Input character filter flags (bitwise OR). */
typedef enum {
    UI_INPUT_HEX   = 0x01, /**< 0-9 a-f A-F x X */
    UI_INPUT_DEC   = 0x02, /**< 0-9 */
    UI_INPUT_ALPHA = 0x04, /**< a-z A-Z */
    UI_INPUT_DOT   = 0x08, /**< . (for filenames) */
    UI_INPUT_PRINT = 0x10, /**< all printable ASCII 0x20-0x7E */
    UI_INPUT_ALNUM = 0x07, /**< HEX | DEC | ALPHA */
    UI_INPUT_FNAME = 0x0F, /**< ALNUM | DOT */
} ui_input_flags_t;

/* ── I/O context ────────────────────────────────────────────────────── */

/**
 * @brief I/O context for popup rendering and input.
 *
 * The popup uses write_out to draw and rx_blocking to read single chars.
 */
typedef struct {
    int (*write_out)(int fd, const void *buf, int count); /**< Terminal write */
    uint8_t cols;  /**< Terminal width */
    uint8_t rows;  /**< Terminal height */
} ui_popup_io_t;

/* ── Popup functions ────────────────────────────────────────────────── */

/**
 * @brief Centered yes/no confirmation dialog.
 *
 * Draws a styled popup box with a title, message, and "Continue? (y/n)"
 * prompt.  Blocks until the user presses y/Y (returns true) or any other
 * key (returns false).
 *
 * @param io       I/O context
 * @param title    Title shown in the popup (may be NULL for no title)
 * @param message  Body message text
 * @param style    Visual style (UI_POPUP_INFO, _WARN, or _DANGER)
 * @return true if user confirmed (y/Y), false otherwise
 */
bool ui_popup_confirm(const ui_popup_io_t *io,
                      const char *title,
                      const char *message,
                      ui_popup_style_t style);

/**
 * @brief Centered text input popup.
 *
 * Draws a styled popup with a title, prompt, and editable text field.
 * Supports backspace, Enter (submit), and Escape (cancel).
 *
 * @param io          I/O context
 * @param title       Title shown in the popup
 * @param prompt      Inline prompt text (e.g. "Filename:")
 * @param buf         Output buffer for the entered text
 * @param buf_size    Size of buf (max characters = buf_size - 1)
 * @param flags       Character filter (UI_INPUT_HEX, etc.)
 * @return true if user submitted (Enter), false if cancelled (Esc)
 */
bool ui_popup_text_input(const ui_popup_io_t *io,
                         const char *title,
                         const char *prompt,
                         char *buf,
                         uint8_t buf_size,
                         uint8_t flags);

/**
 * @brief Centered number input popup with hex/decimal parsing.
 *
 * Wraps ui_popup_text_input with automatic strtoul parsing.
 * Accepts "0x" prefix for hex or plain decimal.
 *
 * @param io          I/O context
 * @param title       Title shown in the popup
 * @param default_val Default value displayed in the prompt
 * @param min_val     Minimum accepted value
 * @param max_val     Maximum accepted value
 * @param result      Output: parsed numeric value
 * @return true if user submitted a valid number, false if cancelled/invalid
 */
bool ui_popup_number(const ui_popup_io_t *io,
                     const char *title,
                     uint32_t default_val,
                     uint32_t min_val,
                     uint32_t max_val,
                     uint32_t *result);

/* ── Progress / operation popup ─────────────────────────────────────── */

/**
 * @brief Live state for a persistent operation-progress popup.
 *
 * Allocated on-stack by the caller.  The popup draws a bordered box with:
 *   Row 0: title (what operation is running)
 *   Row 1: progress bar  [####          ] 34%
 *   Row 2: message text (latest op_message)
 *   Row 3: warning text (yellow, sticky)
 *   Row 4: error text (red, sticky)
 *   Row 5: result text (shown after finish, green/red)
 *   Row 6: "Press any key..." hint
 *
 * All rows update in-place via the update functions below.
 */
typedef struct {
    const ui_popup_io_t *io;
    int left, top, width, height;
    /* Row indices (terminal rows, 1-based) */
    int row_title, row_progress, row_message, row_warning, row_error, row_result, row_hint;
    uint32_t prog_cur, prog_total;
    bool msg_set;    /**< true after first message drawn (locks the row) */
    bool finished;   /**< true after _finish() is called */
    bool success;
} ui_popup_progress_t;

/**
 * @brief Open a progress popup.
 *
 * Draws the initial bordered box.  The caller then drives updates
 * via the _set_* functions during the operation.
 */
void ui_popup_progress_open(ui_popup_progress_t *pp,
                            const ui_popup_io_t *io,
                            const char *title);

/** Update the progress bar (0..total). */
void ui_popup_progress_set_progress(ui_popup_progress_t *pp,
                                    uint32_t current, uint32_t total);

/** Update the message row (latest status text). */
void ui_popup_progress_set_message(ui_popup_progress_t *pp, const char *msg);

/** Set warning text (sticky — persists until replaced). */
void ui_popup_progress_set_warning(ui_popup_progress_t *pp, const char *msg);

/** Set error text (sticky). */
void ui_popup_progress_set_error(ui_popup_progress_t *pp, const char *msg);

/**
 * @brief Mark the operation as finished and show a result line.
 *
 * Changes the popup border to green (success) or red (failure) and
 * adds "Press any key..." at the bottom.
 */
void ui_popup_progress_finish(ui_popup_progress_t *pp,
                              bool success, const char *result_msg);

/**
 * @brief Block until the user presses a key, then return.
 *
 * Call after _finish().  Returns true if the user pressed Escape
 * ("cancel"), false for any other key ("ok / acknowledge").
 */
bool ui_popup_progress_wait(ui_popup_progress_t *pp);

#endif /* UI_POPUP_H */
