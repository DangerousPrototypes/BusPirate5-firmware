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
#include "system_monitor.h"
#include "display/scope.h"
#include "pirate/intercore_helpers.h"
#include "tusb.h"
#include "pirate/psu.h"
#include "ui/ui_toolbar.h"
#include "ui/ui_pin_render.h"

/* Height of the status bar in terminal lines */
#define STATUSBAR_HEIGHT 4

/* Toolbar descriptor for this statusbar — registered in ui_statusbar_init(). */
static toolbar_t statusbar_toolbar = {
    .name    = "statusbar",
    .height  = STATUSBAR_HEIGHT,
    .enabled = false,
    .owner_data = NULL,
    .draw    = NULL, /* draw handled by ui_statusbar_update_from_core1 */
    .update  = NULL,
    .destroy = NULL,
};

uint32_t ui_statusbar_info(char* buf, size_t buffLen) {
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
    len += snprintf(&buf[len], buffLen - len, "\033[%dX", system_config.terminal_ansi_columns - cnt);
    len += snprintf(&buf[len], buffLen - len, "%s", ui_term_color_reset()); // snprintf to buffer
    return len;
}

void ui_statusbar_update_blocking() {
    BP_ASSERT_CORE0(); // if called from core1, this will deadlock
    if(!tud_cdc_n_connected(0)) return;
    system_config.terminal_ansi_statusbar_update = true;
    icm_core0_send_message_synchronous(BP_ICM_UPDATE_STATUS_BAR);
}

void ui_statusbar_update_from_core1(uint32_t update_flags) {
    BP_ASSERT_CORE1();
    if(!tud_cdc_n_connected(0)) return;
    uint32_t len = 0;
    size_t buffLen = sizeof(tx_sb_buf);

    if (!update_flags) // nothing to update
    {
        return;
    }

    /* Use the toolbar registry to get the start row of this bar. */
    uint16_t start_row = toolbar_get_start_row(&statusbar_toolbar);
    if (start_row == 0) {
        /* Not registered / disabled — nothing to draw. */
        return;
    }

    // save cursor, hide cursor
    len += snprintf(&tx_sb_buf[len], buffLen - len, "\0337\033[?25l");

    // print each line of the toolbar
    if (update_flags & UI_UPDATE_INFOBAR) {
        monitor_force_update(); // we want to repaint the whole screen if we're doing the pin names...
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row);
        len += ui_statusbar_info(&tx_sb_buf[len], buffLen - len);
    }

    if (update_flags & UI_UPDATE_NAMES) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row + 1);
        len += ui_pin_render_names(&tx_sb_buf[len], buffLen - len);
    }

    if ((update_flags & UI_UPDATE_CURRENT) && !(update_flags & UI_UPDATE_LABELS)) // show current under Vout
    {
        char* c;
        if (monitor_get_current_ptr(&c)) {
            len += snprintf(&tx_sb_buf[len],
                            buffLen - len,
                            "\033[%d;0H%s%s%smA",
                            start_row + 2,
                            ui_term_color_num_float(),
                            c,
                            ui_term_color_reset());
        }
    }

    if (update_flags & UI_UPDATE_LABELS) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row + 2);
        len += ui_pin_render_labels(&tx_sb_buf[len], buffLen - len);
    }

    if (update_flags & UI_UPDATE_VOLTAGES) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[%d;0H", start_row + 3);
        len += ui_pin_render_values(&tx_sb_buf[len], buffLen - len, false);
    }

    // restore cursor, show cursor
    len += snprintf(&tx_sb_buf[len], buffLen - len, "\0338");

    if (!system_config.terminal_hide_cursor) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\033[?25h");
    }

    tx_sb_start(len);
}

void ui_statusbar_init(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        statusbar_toolbar.enabled = true;
        toolbar_register(&statusbar_toolbar);
        toolbar_apply_scroll_region();
    }
}

void ui_statusbar_deinit(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        system_config.terminal_ansi_statusbar = 0;
        system_config.terminal_ansi_statusbar_update = false;
        busy_wait_ms(100); // wait for the last statusbar update to finish

        uint16_t start_row = toolbar_get_start_row(&statusbar_toolbar);
        ui_term_cursor_save_printf();
        ui_term_scroll_region_printf(1, system_config.terminal_ansi_rows); // disable region block

        if (start_row > 0) {
            for (uint8_t i = 0; i < STATUSBAR_HEIGHT; i++) {
                printf("\033[%d;0H\033[K", start_row + i); // clear each bar line
            }
        }

        ui_term_cursor_restore_printf();
        toolbar_unregister(&statusbar_toolbar);
        statusbar_toolbar.enabled = false;
    }
}
