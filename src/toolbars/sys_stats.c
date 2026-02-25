/**
 * @file sys_stats.c
 * @brief 1-line system statistics toolbar implementation.
 * @details Displays uptime, PSU on/off, USB state, and registered toolbar count.
 *          Refreshes on every draw call.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "pirate/psu.h"
#include "tusb.h"

#define SYS_STATS_HEIGHT 1

/* Forward declarations */
static void sys_stats_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width);
static uint32_t sys_stats_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                          uint16_t start_row, uint16_t width,
                                          uint32_t update_flags);

static toolbar_t sys_stats_toolbar = {
    .name       = "sys_stats",
    .height     = SYS_STATS_HEIGHT,
    .enabled    = false,
    .owner_data = NULL,
    .draw       = sys_stats_draw_cb,
    .update     = NULL,
    .update_core1 = sys_stats_update_core1_cb,
    .destroy    = NULL,
};

/**
 * @brief .draw callback — paints a single status line.
 */
static void sys_stats_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    (void)tb; (void)width;

    toolbar_draw_prepare();
    ui_term_cursor_save();

    ui_term_cursor_position(start_row, 0);
    ui_term_erase_line();

    /* Uptime */
    uint32_t ms = to_ms_since_boot(get_absolute_time());
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;
    secs %= 60;
    mins %= 60;

    /* PSU state */
    const char* psu_str = psu_status.enabled ? "ON" : "OFF";
    uint32_t psu_bg = psu_status.enabled ? 0x006600 : 0x333333;

    /* USB state */
    bool usb_ok = tud_cdc_n_connected(0);
    const char* usb_str = usb_ok ? "USB" : "---";
    uint32_t usb_bg = usb_ok ? 0x004466 : 0x333333;

    /* Build the line */
    ui_term_color_text_background(0xCCCCCC, 0x222222);
    printf(" Up %lu:%02lu:%02lu ", (unsigned long)hrs, (unsigned long)mins, (unsigned long)secs);

    ui_term_color_text_background(0xFFFFFF, psu_bg);
    printf(" PSU:%s ", psu_str);

    ui_term_color_text_background(0xFFFFFF, usb_bg);
    printf(" %s ", usb_str);

    ui_term_color_text_background(0xCCCCCC, 0x222222);
    printf(" Bars:%u/%u ", toolbar_count_registered(), TOOLBAR_MAX_COUNT);

    /* Fill rest of line with background */
    printf("%s", ui_term_color_reset());

    ui_term_cursor_restore();
    toolbar_draw_release();
}

/**
 * @brief Core1 periodic update callback — renders stats into buffer.
 * @details Uses only snprintf + _buf() variants.  Cursor save/hide/restore/show
 *          are handled by the state machine in ui_toolbar.c.
 */
static uint32_t sys_stats_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                          uint16_t start_row, uint16_t width,
                                          uint32_t update_flags) {
    (void)tb; (void)width; (void)update_flags;
    uint32_t len = 0;

    len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row, 0);
    len += ui_term_erase_line_buf(&buf[len], buf_len - len);

    /* Uptime */
    uint32_t ms   = to_ms_since_boot(get_absolute_time());
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;
    secs %= 60;
    mins %= 60;

    /* PSU state */
    const char* psu_str = psu_status.enabled ? "ON" : "OFF";
    uint32_t psu_bg = psu_status.enabled ? 0x006600 : 0x333333;

    /* USB state */
    bool usb_ok = tud_cdc_n_connected(0);
    const char* usb_str = usb_ok ? "USB" : "---";
    uint32_t usb_bg = usb_ok ? 0x004466 : 0x333333;

    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, 0xCCCCCC, 0x222222);
    len += snprintf(&buf[len], buf_len - len, " Up %lu:%02lu:%02lu ",
                    (unsigned long)hrs, (unsigned long)mins, (unsigned long)secs);

    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, 0xFFFFFF, psu_bg);
    len += snprintf(&buf[len], buf_len - len, " PSU:%s ", psu_str);

    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, 0xFFFFFF, usb_bg);
    len += snprintf(&buf[len], buf_len - len, " %s ", usb_str);

    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, 0xCCCCCC, 0x222222);
    len += snprintf(&buf[len], buf_len - len, " Bars:%u/%u ",
                    toolbar_count_registered(), TOOLBAR_MAX_COUNT);

    len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());

    return len;
}

bool sys_stats_start(void) {
    if (sys_stats_toolbar.enabled) {
        return true; /* already active */
    }
    /* Push content up before shrinking scroll region */
    for (uint16_t i = 0; i < SYS_STATS_HEIGHT; i++) {
        printf("\r\n");
    }
    if (!toolbar_activate(&sys_stats_toolbar)) {
        return false;
    }
    /* Reposition cursor within the new scroll region */
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    sys_stats_draw_cb(&sys_stats_toolbar,
                      toolbar_get_start_row(&sys_stats_toolbar),
                      system_config.terminal_ansi_columns);
    return true;
}

void sys_stats_stop(void) {
    if (!sys_stats_toolbar.enabled) {
        return;
    }
    toolbar_teardown(&sys_stats_toolbar);
}

bool sys_stats_is_active(void) {
    return sys_stats_toolbar.enabled;
}

