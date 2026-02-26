/**
 * @file ui_pin_render.h
 * @brief Unified pin name / label / voltage rendering.
 * @details All rendering always writes into a caller-provided buffer via
 *          snprintf — never calls printf directly.  Behaviour is customised
 *          through @ref pin_render_flags_t flags.
 *
 *          Core0 callers (v/V command) pass a stack-local buffer, then push
 *          it through tx_fifo_write().  Core1 callers (statusbar) pass a
 *          slice of tx_tb_buf and commit via tx_tb_start().
 *
 *          Usage – v command (Core0):
 *          @code
 *          char tmp[512];
 *          uint32_t len;
 *          pin_render_flags_t f = PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS;
 *          len  = ui_pin_render_names(tmp, sizeof(tmp), f);
 *          tx_fifo_write(tmp, len);
 *          len  = ui_pin_render_labels(tmp, sizeof(tmp), f);
 *          tx_fifo_write(tmp, len);
 *          len  = ui_pin_render_values(tmp, sizeof(tmp), f);
 *          tx_fifo_write(tmp, len);
 *          @endcode
 *
 *          Usage – statusbar (Core1):
 *          @code
 *          pin_render_flags_t sf = PIN_RENDER_CHANGE_TRACK | PIN_RENDER_CLEAR_CELLS;
 *          len += ui_pin_render_names(&tx_tb_buf[len], rem, sf);
 *          len += ui_pin_render_labels(&tx_tb_buf[len], rem, sf);
 *          len += ui_pin_render_values(&tx_tb_buf[len], rem, sf);
 *          @endcode
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Flags controlling per-use rendering behaviour.
 *
 * Callers combine these to get the exact output they need.
 * - Core0 (v command): PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS
 * - Core1 (statusbar): PIN_RENDER_CHANGE_TRACK | PIN_RENDER_CLEAR_CELLS
 */
typedef enum {
    PIN_RENDER_CHANGE_TRACK  = (1u << 0),  /**< Skip unchanged cells (emit bare \\t). Core1 only. */
    PIN_RENDER_NEWLINE       = (1u << 1),  /**< Append \\r\\n at end of row. */
    PIN_RENDER_CLEAR_CELLS   = (1u << 2),  /**< Prepend \\033[8X (erase 8 cols) before each cell. */
} pin_render_flags_t;

/**
 * @brief Render the pin-name row ("1.IO0  2.IO1  ...").
 * @param buf      Output buffer (must not be NULL).
 * @param buf_len  Buffer capacity.
 * @param flags    Rendering flags.
 * @return Bytes written to buf.
 */
uint32_t ui_pin_render_names(char* buf, size_t buf_len, pin_render_flags_t flags);

/**
 * @brief Render the pin-label row (mode-assigned labels, current mA, etc.).
 * @param buf      Output buffer (must not be NULL).
 * @param buf_len  Buffer capacity.
 * @param flags    Rendering flags.
 * @return Bytes written to buf.
 */
uint32_t ui_pin_render_labels(char* buf, size_t buf_len, pin_render_flags_t flags);

/**
 * @brief Render the pin-voltage row (V, mA, Hz, GND, etc.).
 * @param buf      Output buffer (must not be NULL).
 * @param buf_len  Buffer capacity.
 * @param flags    Rendering flags.
 * @return Bytes written to buf (0 when CHANGE_TRACK set and nothing changed).
 */
uint32_t ui_pin_render_values(char* buf, size_t buf_len, pin_render_flags_t flags);

/**
 * @brief Force a full repaint on next CHANGE_TRACK render by invalidating shadows.
 *
 * Call this when an external event (e.g. info-bar change) means every cell
 * must be re-emitted regardless of whether the underlying value changed.
 * Only meaningful for Core1 (statusbar) which uses PIN_RENDER_CHANGE_TRACK.
 */
void ui_pin_render_reset_shadows(void);
