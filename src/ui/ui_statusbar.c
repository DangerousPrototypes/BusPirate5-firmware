#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "usb_tx.h"
#include "ui/ui_flags.h"
#include "display/scope.h"
#include "tusb.h"
#include "pirate/psu.h"
#include "ui/ui_toolbar.h"
#include "ui/ui_pin_render.h"
#include "system_monitor.h"

/* Height of the status bar in terminal lines */
#define STATUSBAR_HEIGHT 4

/* Forward declaration — Core1 content callback, defined below. */
static uint32_t statusbar_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                          uint16_t start_row, uint16_t width,
                                          uint32_t update_flags);

/**
 * @brief .draw callback — triggers a blocking full repaint via Core1.
 * @details Called from toolbar_redraw_all() on Core0.  Sends an ICM message
 *          to Core1 which renders into tx_tb_buf and drains to USB.
 */
static void statusbar_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    (void)tb; (void)start_row; (void)width;
    toolbar_update_blocking();
}

/* Toolbar descriptor for this statusbar — registered in ui_statusbar_init(). */
static toolbar_t statusbar_toolbar = {
    .name    = "statusbar",
    .height  = STATUSBAR_HEIGHT,
    .enabled = false,
    .anchor_bottom = true,
    .owner_data = NULL,
    .draw    = statusbar_draw_cb,
    .update_core1 = statusbar_update_core1_cb,
    .destroy = NULL,
};

static uint32_t ui_statusbar_info(char* buf, size_t buffLen) {
    uint32_t len = 0;
    uint32_t temp = 0;
    uint32_t cnt = 0;

    len += ui_term_color_text_background_buf(&buf[len], buffLen - len, 0x000000, BP_COLOR_GREY);

    if (psu_status.enabled) {
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Vout: %u.%uV",
                        (psu_status.voltage_actual_int) / 10000,
                        ((psu_status.voltage_actual_int) % 10000) / 100);
        len += temp;
        cnt += temp;

        if (!psu_status.current_limit_override) {
            temp = snprintf(&buf[len],
                            buffLen - len,
                            "/%u.%umA max",
                            (psu_status.current_actual_int) / 10000,
                            ((psu_status.current_actual_int) % 10000) / 100);
            len += temp;
            cnt += temp;
        }

        if(!psu_status.undervoltage_limit_override){
            temp = snprintf(&buf[len],
                            buffLen - len,
                            "/%u.%uV min",
                            (psu_status.undervoltage_limit_int) / 10000,
                            ((psu_status.undervoltage_limit_int) % 10000) / 100);
            len += temp;
            cnt += temp;
        }
        temp = snprintf(&buf[len], buffLen - len, " | ");
        len += temp;
        cnt += temp;
    }

    if (psu_status.error_overcurrent) {
        // show Power Supply: ERROR
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Vout: ERROR > %u.%umA | ",
                        (psu_status.current_actual_int) / 10000,
                        ((psu_status.current_actual_int) % 10000) / 100);
        len += temp;
        cnt += temp;
    }else if (psu_status.error_undervoltage){
        // show Power Supply: ERROR
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Vout: ERROR < %u.%uV | ",
                        (psu_status.undervoltage_limit_int) / 10000,
                        ((psu_status.undervoltage_limit_int) % 10000) / 100);
        len += temp;
        cnt += temp;
    }

    if (system_config.pullup_enabled) {
        // show Pull-up resistors ON
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Pull-ups: ON | ");
        len += temp;
        cnt += temp;
    }
    if (scope_running) { // scope is using the analog subsystem
        temp = snprintf(&buf[len], buffLen - len, "V update slower when scope running");
        len += temp;
        cnt += temp;
    }
    // fill in blank space
    len += ui_term_erase_chars_buf(&buf[len], buffLen - len, system_config.terminal_ansi_columns - cnt);
    len += snprintf(&buf[len], buffLen - len, "%s", ui_term_color_reset()); // snprintf to buffer
    return len;
}

/**
 * @brief Core1 content callback — renders statusbar rows into caller buffer.
 * @details Cursor save/hide/restore/show are handled by the state machine.
 */
static uint32_t statusbar_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                          uint16_t start_row, uint16_t width,
                                          uint32_t update_flags) {
    (void)tb; (void)width;

    if (!update_flags) return 0;
    if (start_row == 0) return 0;

    uint32_t len = 0;
    pin_render_flags_t sb_flags = PIN_RENDER_CHANGE_TRACK | PIN_RENDER_CLEAR_CELLS;

    if (update_flags & UI_UPDATE_INFOBAR) {
        monitor_force_update();
        len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row, 0);
        len += ui_statusbar_info(&buf[len], buf_len - len);
    }

    if (update_flags & UI_UPDATE_NAMES) {
        len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row + 1, 0);
        len += ui_pin_render_names(&buf[len], buf_len - len, sb_flags);
    }

    if ((update_flags & UI_UPDATE_CURRENT) && !(update_flags & UI_UPDATE_LABELS)) {
        len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row + 2, 0);
        len += ui_pin_render_labels(&buf[len], buf_len - len, sb_flags);
    }

    if (update_flags & UI_UPDATE_LABELS) {
        len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row + 2, 0);
        len += ui_pin_render_labels(&buf[len], buf_len - len, sb_flags);
    }

    if (update_flags & UI_UPDATE_VOLTAGES) {
        len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row + 3, 0);
        len += ui_pin_render_values(&buf[len], buf_len - len, sb_flags);
    }

    return len;
}

void ui_statusbar_init(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        /* Push content up to make room (harmless on fresh-screen init) */
        for (uint16_t i = 0; i < STATUSBAR_HEIGHT; i++) {
            printf("\r\n");
        }
        toolbar_activate(&statusbar_toolbar);
        /* Reposition cursor within the new scroll region */
        ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    }
}

void ui_statusbar_deinit(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        system_config.terminal_ansi_statusbar = 0;
        toolbar_teardown(&statusbar_toolbar);
    }
}
