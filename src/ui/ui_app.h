/**
 * @file ui_app.h
 * @brief Fullscreen app scaffold — alt-screen lifecycle & shared I/O helpers.
 *
 * Eliminates the 8-line boilerplate (toolbar_draw_prepare → alt-screen →
 * clear → app → restore) duplicated across every fullscreen application.
 *
 * Usage:
 * @code
 *   ui_app_t app;
 *   ui_app_open(&app);          // alt-screen, toolbar pause, drain, key init
 *   while (running) {
 *       int key = ui_app_read_key(&app);
 *       ui_app_write_str(&app, "hello");
 *       ...
 *   }
 *   ui_app_close(&app);         // restore terminal
 * @endcode
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef UI_APP_H
#define UI_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "lib/vt100_keys/vt100_keys.h"

/**
 * @brief Fullscreen app context — stack-allocate one per app.
 */
typedef struct {
    uint8_t rows;           /**< Terminal height from system_config */
    uint8_t cols;           /**< Terminal width from system_config */
    vt100_key_state_t keys; /**< VT100 key decoder state */
} ui_app_t;

/* ── Lifecycle ──────────────────────────────────────────────────────── */

/**
 * @brief Enter fullscreen mode.
 *
 * Pauses Core 1 toolbar updates, enters VT100 alt-screen buffer,
 * resets scroll region, clears screen, homes cursor, drains stale
 * rx bytes, snapshots terminal dimensions, and initialises the key
 * decoder.
 */
void ui_app_open(ui_app_t *app);

/**
 * @brief Leave fullscreen mode.
 *
 * Drains stale rx bytes, leaves alt-screen buffer, re-applies the
 * toolbar scroll region, positions cursor, and resumes Core 1 updates.
 */
void ui_app_close(ui_app_t *app);

/* ── I/O helpers ────────────────────────────────────────────────────── */

/** Blocking decoded key read (returns ASCII or VT100_KEY_*). */
int ui_app_read_key(ui_app_t *app);

/** Push a decoded key back so the next read_key returns it. */
void ui_app_unget_key(ui_app_t *app, int key);

/** Write a NUL-terminated string to the terminal. */
void ui_app_write_str(const char *s);

/** Write a buffer of known length to the terminal. */
void ui_app_write_buf(const void *buf, int len);

/** Drain all pending bytes from the rx FIFO. */
void ui_app_drain_rx(void);

/* ── Callbacks for vt100_menu / ui_file_picker integration ────────── */

/**
 * write_out callback matching the signature expected by vt100_menu
 * and ui_file_picker: int (*)(int fd, const void *buf, int count).
 * The fd parameter is ignored.
 */
int ui_app_write_out_cb(int fd, const void *buf, int count);

#endif /* UI_APP_H */
