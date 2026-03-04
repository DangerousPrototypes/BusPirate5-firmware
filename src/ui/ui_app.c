/**
 * @file ui_app.c
 * @brief Fullscreen app scaffold — alt-screen lifecycle & shared I/O helpers.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_app.h"

/* ── Internal I/O adapters for vt100_key_state_t ────────────────────── */

static int app_read_blocking(char *c) {
    rx_fifo_get_blocking(c);
    return 1;
}

static int app_read_try(char *c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

/* ── Lifecycle ──────────────────────────────────────────────────────── */

void ui_app_open(ui_app_t *app) {
    memset(app, 0, sizeof(*app));
    app->rows = (uint8_t)system_config.terminal_ansi_rows;
    app->cols = (uint8_t)system_config.terminal_ansi_columns;

    /* Pause Core 1 toolbar updates to avoid VT100 interleaving */
    toolbar_draw_prepare();

    /* Enter alt-screen buffer */
    printf("\x1b[?1049h");  /* smcup */
    printf("\x1b[r");       /* reset scroll region */
    printf("\x1b[2J");      /* clear screen */
    printf("\x1b[H");       /* cursor home */

    /* Drain stale rx bytes */
    ui_app_drain_rx();

    /* Init key decoder */
    vt100_key_init(&app->keys, app_read_blocking, app_read_try);
}

void ui_app_close(ui_app_t *app) {
    (void)app;

    /* Drain stale rx bytes */
    ui_app_drain_rx();

    /* Leave alt-screen buffer */
    printf("\x1b[?1049l");  /* rmcup */

    /* Restore toolbar scroll region and position cursor */
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();
}

/* ── I/O helpers ────────────────────────────────────────────────────── */

int ui_app_read_key(ui_app_t *app) {
    return vt100_key_read(&app->keys);
}

void ui_app_unget_key(ui_app_t *app, int key) {
    vt100_key_unget(&app->keys, key);
}

void ui_app_write_str(const char *s) {
    tx_fifo_write(s, strlen(s));
}

void ui_app_write_buf(const void *buf, int len) {
    tx_fifo_write((const char *)buf, (uint32_t)len);
}

void ui_app_drain_rx(void) {
    char c;
    while (rx_fifo_try_get(&c)) {}
}

int ui_app_write_out_cb(int fd, const void *buf, int count) {
    (void)fd;
    tx_fifo_write((const char *)buf, (uint32_t)count);
    return count;
}
