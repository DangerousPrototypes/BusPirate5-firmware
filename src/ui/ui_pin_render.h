/**
 * @file ui_pin_render.h
 * @brief Unified pin name / label / voltage rendering.
 * @details Provides a single implementation of the three pin-info rows that
 *          is shared between the `v`/`V` command (direct printf) and the status
 *          bar (snprintf to buffer).  Eliminates the previously duplicate
 *          implementations in ui_info.c and ui_statusbar.c.
 *
 *          Usage – printf mode (pass NULL buf):
 *          @code
 *          ui_pin_render_names(NULL, 0, NULL);
 *          ui_pin_render_labels(NULL, 0, NULL);
 *          ui_pin_render_values(NULL, 0, NULL, false);
 *          @endcode
 *
 *          Usage – buffer mode (status bar):
 *          @code
 *          uint32_t len = 0;
 *          len += ui_pin_render_names(buf + len, bufsize - len, NULL);
 *          len += ui_pin_render_labels(buf + len, bufsize - len, NULL);
 *          len += ui_pin_render_values(buf + len, bufsize - len, NULL, false);
 *          @endcode
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Render the pin-name row ("1.IO0  2.IO1  ...").
 * @param buf      Output buffer, or NULL to printf directly.
 * @param buf_len  Buffer capacity (ignored when buf is NULL).
 * @return Bytes written to buf (0 when buf is NULL).
 */
uint32_t ui_pin_render_names(char* buf, size_t buf_len);

/**
 * @brief Render the pin-label row (mode-assigned labels, current, etc.).
 * @param buf      Output buffer, or NULL to printf directly.
 * @param buf_len  Buffer capacity (ignored when buf is NULL).
 * @return Bytes written to buf (0 when buf is NULL).
 */
uint32_t ui_pin_render_labels(char* buf, size_t buf_len);

/**
 * @brief Render the pin-voltage row (V, mA, Hz, etc.).
 * @param buf      Output buffer, or NULL to printf directly.
 * @param buf_len  Buffer capacity (ignored when buf is NULL).
 * @param refresh  When true and buf is NULL, omit the trailing newline
 *                 (used for in-place refresh in continuous freq display).
 * @return Bytes written to buf (0 when buf is NULL / no update needed).
 */
uint32_t ui_pin_render_values(char* buf, size_t buf_len, bool refresh);
