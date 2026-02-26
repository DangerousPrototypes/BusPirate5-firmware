/**
 * @file sys_stats.c
 * @brief 1-line system statistics toolbar implementation.
 * @details Displays PSU, pull-up, binmode status indicators on the left,
 *          and uptime right-justified.  All rendering goes through the
 *          Core1 _buf() path.
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
#include "binmode/binmodes.h"

#define SYS_STATS_HEIGHT 1

/* Forward declarations */
static uint32_t sys_stats_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                          uint16_t start_row, uint16_t width,
                                          uint32_t update_flags);

static const toolbar_def_t sys_stats_toolbar_def = {
    .name         = "sys_stats",
    .height       = SYS_STATS_HEIGHT,
    .anchor_bottom = false,
    .draw         = NULL, /* Core1-rendered: toolbar_redraw_all() auto-delegates */
    .update_core1 = sys_stats_update_core1_cb,
    .destroy      = NULL,
};

static toolbar_t sys_stats_toolbar = {
    .def        = &sys_stats_toolbar_def,
    .height     = SYS_STATS_HEIGHT,
    .enabled    = false,
    .owner_data = NULL,
};

/**
 * @brief Core1 update callback — renders stats into buffer.
 * @details Layout (left → right):
 *          [PSU 8ch] [PULLUP 8ch] [BINMODE: name 16+ch] ... gap ... [Up H:MM:SS]
 *          Each indicator: white text on grey-blue (on) or grey (off).
 *          Gap & uptime: light-grey text on dark background.
 */
static uint32_t sys_stats_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                          uint16_t start_row, uint16_t width,
                                          uint32_t update_flags) {
    (void)tb; (void)update_flags;
    uint32_t len = 0;
    int n;

    len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row, 0);

    /* ---- Indicator colours ---- */
    const uint32_t fg_ind   = 0xFFFFFF;  /* white text on indicators  */
    const uint32_t bg_on    = 0x004466;  /* grey-blue when active     */
    const uint32_t bg_off   = 0x333333;  /* grey when inactive        */
    const uint32_t fg_dark  = 0xCCCCCC;  /* light grey for gap/uptime */
    const uint32_t bg_dark  = 0x222222;  /* dark background           */

    /* Track visible columns for gap calculation */
    uint32_t cols = 0;

    /* ---- PSU indicator (1 column = 8 chars) ---- */
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len,
                                             fg_ind, psu_status.enabled ? bg_on : bg_off);
    n = snprintf(&buf[len], buf_len - len, " %-7s", "PSU");
    len += (uint32_t)n;
    cols += (uint32_t)n;

    /* ---- FUSE indicator (1 column = 8 chars) ---- */
    /* Red when overcurrent or undervoltage fault, green when PSU on + OK, grey when PSU off */
    const uint32_t bg_fuse_err = 0x660000;  /* red background on fault */
    const uint32_t bg_fuse_ok  = 0x006600;  /* green background, fuse OK */
    bool fuse_fault = psu_status.error_overcurrent || psu_status.error_undervoltage;
    uint32_t fuse_bg = fuse_fault ? bg_fuse_err
                     : psu_status.enabled ? bg_fuse_ok
                     : bg_off;
    const char* fuse_str = psu_status.error_overcurrent ? "OC"
                         : psu_status.error_undervoltage ? "UV"
                         : "OK";
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, fg_ind, fuse_bg);
    n = snprintf(&buf[len], buf_len - len, " FUSE:%-2s", fuse_str);
    len += (uint32_t)n;
    cols += (uint32_t)n;

    /* ---- PULLUP indicator (1 column = 8 chars) ---- */
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len,
                                             fg_ind, system_config.pullup_enabled ? bg_on : bg_off);
    n = snprintf(&buf[len], buf_len - len, " %-7s", "PULLUP");
    len += (uint32_t)n;
    cols += (uint32_t)n;

    /* ---- BINMODE indicator: colored label + dark-bg name ---- */
    uint8_t bm_idx = system_config.binmode_select;
    if (bm_idx >= BINMODE_MAXPROTO) { bm_idx = 0; }
    const char* bm_name = binmodes[bm_idx].binmode_name;
    bool bm_active = tud_cdc_n_connected(1);

    /* Colored "BINMODE:" label (10 chars: space + BINMODE: + space) */
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len,
                                             fg_ind, bm_active ? bg_on : bg_off);
    n = snprintf(&buf[len], buf_len - len, " BINMODE: ");
    len += (uint32_t)n;
    cols += (uint32_t)n;

    /* Binmode name on dark background, padded to next 8-char boundary */
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, fg_dark, bg_dark);
    char bm_name_buf[40];
    int bm_name_len = snprintf(bm_name_buf, sizeof(bm_name_buf), "%s ", bm_name);
    int bm_total = 10 + bm_name_len;  /* label + name */
    int bm_col = ((bm_total + 7) / 8) * 8;
    if (bm_col < 16) { bm_col = 16; }
    int bm_pad = bm_col - 10;  /* remaining chars for name field */
    n = snprintf(&buf[len], buf_len - len, "%-*s", bm_pad, bm_name_buf);
    len += (uint32_t)n;
    cols += (uint32_t)n;

    /* ---- Uptime string (pre-format so we know its width) ---- */
    uint32_t ms   = to_ms_since_boot(get_absolute_time());
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hrs  = mins / 60;
    secs %= 60;
    mins %= 60;

    char uptime[24];
    int uptime_len = snprintf(uptime, sizeof(uptime), " Up %lu:%02lu:%02lu ",
                              (unsigned long)hrs, (unsigned long)mins, (unsigned long)secs);

    /* ---- Dark gap (fills space between indicators and uptime) ---- */
    uint32_t right_cols = cols + (uint32_t)uptime_len;
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, fg_dark, bg_dark);
    if (right_cols < width) {
        uint32_t gap = width - right_cols;
        for (uint32_t i = 0; i < gap; i++) {
            if (len < buf_len - 1) { buf[len++] = ' '; }
        }
    }

    /* ---- Uptime (right-justified, dark background) ---- */
    n = snprintf(&buf[len], buf_len - len, "%s", uptime);
    len += (uint32_t)n;

    len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());

    return len;
}

bool sys_stats_start(void) {
    if (sys_stats_toolbar.enabled) {
        return true; /* already active */
    }
    return toolbar_activate(&sys_stats_toolbar);
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

