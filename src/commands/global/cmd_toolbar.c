/**
 * @file cmd_toolbar.c
 * @brief Toolbar debug/test command — for development/testing only.
 * @details Provides commands to inspect and test the toolbar registry:
 *            toolbar list              — show all registered toolbars
 *            toolbar test <height>     — create a coloured test toolbar
 *            toolbar remove            — remove the test toolbar
 *            toolbar stats             — toggle 1-line sys_stats toolbar
 *            toolbar pins              — toggle 2-line pin_watcher toolbar
 *            toolbar statusbar         — toggle the 4-line status bar
 *
 * The test toolbar is a simple block of coloured rows showing its name and
 * row numbers, useful for verifying scroll-region math, stacking order,
 * erase, teardown, and resize behaviour.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "ui/ui_toolbar.h"
#include "toolbars/sys_stats.h"
#include "toolbars/pin_watcher.h"
#include "ui/ui_statusbar.h"
#include "lib/bp_args/bp_cmd.h"

/* ── action verbs ────────────────────────────────────────────────────────── */

enum toolbar_actions {
    TOOLBAR_LIST = 1,
    TOOLBAR_TEST,
    TOOLBAR_REMOVE,
    TOOLBAR_STATS,
    TOOLBAR_PINS,
    TOOLBAR_STATUSBAR,
};

static const bp_command_action_t toolbar_action_defs[] = {
    { TOOLBAR_LIST,      "list",      0x00 },
    { TOOLBAR_TEST,      "test",      0x00 },
    { TOOLBAR_REMOVE,    "remove",    0x00 },
    { TOOLBAR_STATS,     "stats",     0x00 },
    { TOOLBAR_PINS,      "pins",      0x00 },
    { TOOLBAR_STATUSBAR, "statusbar", 0x00 },
};

/* ── options ─────────────────────────────────────────────────────────────── */

static const bp_command_opt_t toolbar_opts[] = {
    { 0 }   /* none for now */
};

/* ── usage ───────────────────────────────────────────────────────────────── */

static const char* const usage[] = {
    "toolbar [list|test|remove|stats|pins|statusbar]",
    "List registered toolbars:%s toolbar list",
    "Create a test toolbar (1-8 lines):%s toolbar test 3",
    "Remove test toolbar:%s toolbar remove",
    "Toggle system stats toolbar:%s toolbar stats",
    "Toggle pin watcher toolbar:%s toolbar pins",
    "Toggle status bar:%s toolbar statusbar",
};

/* ── command definition ──────────────────────────────────────────────────── */

const bp_command_def_t toolbar_cmd_def = {
    .name         = "toolbar",
    .description  = 0x00,           /* hidden from main help listing */
    .actions      = toolbar_action_defs,
    .action_count = count_of(toolbar_action_defs),
    .opts         = toolbar_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

/* ── test toolbar state ──────────────────────────────────────────────────── */

#define TEST_TOOLBAR_MAX_HEIGHT 8

static bool test_toolbar_active = false;

static void test_toolbar_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width);

static toolbar_t test_toolbar = {
    .name       = "test",
    .height     = 0,            /* set dynamically */
    .enabled    = false,
    .owner_data = NULL,
    .draw       = test_toolbar_draw_cb,
    .destroy    = NULL,
};

/**
 * @brief .draw callback — paints coloured rows showing toolbar metadata.
 * @details Pure painter — caller handles prepare/release and cursor save/restore.
 */
static void test_toolbar_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    (void)width;

    for (uint16_t i = 0; i < tb->height; i++) {
        ui_term_cursor_position(start_row + i, 0);
        ui_term_erase_line();
        /* Cycle through a few background colours so rows are distinguishable */
        static const uint32_t bg_colors[] = {
            0x004466, 0x006644, 0x664400, 0x440066,
            0x660044, 0x004400, 0x000066, 0x444400,
        };
        uint32_t bg = bg_colors[i % (sizeof(bg_colors) / sizeof(bg_colors[0]))];
        ui_term_color_text_background(0xFFFFFF, bg);
        printf(" TEST toolbar  row %u/%u  (start_row=%u) %s",
               i + 1, tb->height, start_row, ui_term_color_reset());
    }
}

/* ── command handler ─────────────────────────────────────────────────────── */

void toolbar_cmd_handler(struct command_result* res) {
    if (bp_cmd_help_check(&toolbar_cmd_def, res->help_flag)) {
        return;
    }

    if (!system_config.terminal_ansi_color) {
        printf("toolbar command requires VT100 mode\r\n");
        return;
    }

    uint32_t action = 0;
    if (!bp_cmd_get_action(&toolbar_cmd_def, &action)) {
        printf("Usage: toolbar [list|test <height>|remove|stats|pins|statusbar]\r\n");
        return;
    }

    switch (action) {
        /* ── list ──────────────────────────────────────────────────────── */
        case TOOLBAR_LIST: {
            toolbar_print_registry();
            break;
        }

        /* ── test ──────────────────────────────────────────────────────── */
        case TOOLBAR_TEST: {
            if (test_toolbar_active) {
                printf("Test toolbar already active — use 'toolbar remove' first\r\n");
                return;
            }
            uint32_t height = 0;
            if (!bp_cmd_get_positional_uint32(&toolbar_cmd_def, 2, &height)) {
                height = 2;   /* default to 2-line toolbar */
            }
            if (height < 1) height = 1;
            if (height > TEST_TOOLBAR_MAX_HEIGHT) {
                printf("Max height is %d\r\n", TEST_TOOLBAR_MAX_HEIGHT);
                height = TEST_TOOLBAR_MAX_HEIGHT;
            }
            test_toolbar.height = (uint16_t)height;
            if (!toolbar_activate(&test_toolbar)) {
                printf("Registry full — cannot add test toolbar\r\n");
                return;
            }
            test_toolbar_active = true;
            printf("Test toolbar created (%u lines)\r\n", (unsigned)height);
            break;
        }

        /* ── remove ────────────────────────────────────────────────────── */
        case TOOLBAR_REMOVE: {
            if (!test_toolbar_active) {
                printf("No test toolbar active\r\n");
                return;
            }
            toolbar_teardown(&test_toolbar);
            test_toolbar_active = false;
            printf("Test toolbar removed\r\n");
            break;
        }

        /* ── stats ─────────────────────────────────────────────────────── */
        case TOOLBAR_STATS: {
            if (sys_stats_is_active()) {
                sys_stats_stop();
                printf("Stats toolbar removed\r\n");
            } else {
                if (sys_stats_start()) {
                    printf("Stats toolbar started\r\n");
                } else {
                    printf("Registry full — cannot add stats toolbar\r\n");
                }
            }
            break;
        }

        /* ── pins ──────────────────────────────────────────────────────── */
        case TOOLBAR_PINS: {
            if (pin_watcher_is_active()) {
                pin_watcher_stop();
                printf("Pin watcher removed\r\n");
            } else {
                if (pin_watcher_start()) {
                    printf("Pin watcher started\r\n");
                } else {
                    printf("Registry full — cannot add pin watcher\r\n");
                }
            }
            break;
        }

        /* ── statusbar ────────────────────────────────────────────────── */
        case TOOLBAR_STATUSBAR: {
            if (system_config.terminal_ansi_statusbar) {
                ui_statusbar_deinit();
                printf("Status bar disabled\r\n");
            } else {
                system_config.terminal_ansi_statusbar = 1;
                ui_statusbar_init();
                printf("Status bar enabled\r\n");
            }
            break;
        }

        default:
            printf("Unknown action\r\n");
            break;
    }
}
