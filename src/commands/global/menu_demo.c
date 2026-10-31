// ============================================================================
// MENU_DEMO — Teaching Example for VT100 Menu Bar Integration
// ============================================================================
//
// This file is a reference implementation showing how to integrate the
// vt100_menu framework into a Bus Pirate fullscreen application. It
// demonstrates every pattern a developer needs:
//
//   1. Menu item and menu bar definition
//   2. I/O callback wrappers (read_key, write, repaint)
//   3. Key pushback for passthrough keys (e.g. Ctrl-Q while menu is open)
//   4. Direct menu activation via vt100_menu_check_trigger()
//   5. Menu bar drawing in the main loop
//   6. Menu activation via check_trigger in the main loop
//   7. Action dispatch from menu selection
//   8. Alt-screen buffer lifecycle (enter, clear, restore)
//   9. Toolbar pause/resume to prevent VT100 interleaving
//  10. Proper cursor visibility management
//
// Type "menu_demo" at the Bus Pirate prompt to see this command in action.
// Type "menu_demo -h" for help.
//
// Registration:
//   The command is registered in commands.c in the commands[] array:
//     { .command="menu_demo", .allow_hiz=true, .func=&menu_demo_handler,
//       .def=&menu_demo_def, .category=CMD_CAT_HIDDEN }
//
// See also:
//   docs/guides/vt100_menu_guide.md  — full developer documentation
//   src/lib/vt100_menu/vt100_menu.h  — framework API reference
//   src/lib/hx/editor.c              — hex editor integration (complex)
//   src/lib/kilo/kilo.c              — text editor integration (complex)
//
// ============================================================================

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "lib/vt100_menu/vt100_menu.h"
#include "lib/vt100_keys/vt100_keys.h"
#include "lib/bp_args/bp_cmd.h"
#include "menu_demo.h"

// ============================================================================
// USAGE EXAMPLES
// ============================================================================
static const char* const usage[] = {
    "menu_demo",
    "Launch demo:%s menu_demo",
    "",
    "A minimal fullscreen app demonstrating the VT100 menu bar framework.",
    "Press F10 to open the menu. Arrow keys navigate, Enter selects.",
    "Ctrl-Q quits.  Keys pressed while the menu is open pass through.",
};

// ============================================================================
// COMMAND DEFINITION
// ============================================================================
const bp_command_def_t menu_demo_def = {
    .name         = "menu_demo",
    .description  = 0,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals  = NULL,
    .positional_count = 0,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

// ============================================================================
// STEP 1: Define action IDs
// ============================================================================
// Each menu item returns a unique positive integer. Group them by menu
// for readability.  Zero is reserved (MENU_RESULT_NONE).
enum {
    ACT_ABOUT     = 1,
    ACT_HELP      = 2,
    ACT_QUIT      = 10,
    ACT_COLOR_RED = 20,
    ACT_COLOR_GRN = 21,
    ACT_COLOR_BLU = 22,
    ACT_COLOR_RST = 23,
};

// ============================================================================
// STEP 2: Define menu items
// ============================================================================
// Each item: { label, shortcut_hint, action_id, flags }
// Use MENU_ITEM_SEPARATOR for divider lines, MENU_ITEM_DISABLED for greyed-out.
// Terminate with a sentinel (not counted in the menu's .count field).

static const vt100_menu_item_t file_items[] = {
    { "About",  NULL,  ACT_ABOUT, 0 },
    { NULL,     NULL,  0,         MENU_ITEM_SEPARATOR },
    { "Quit",   "^Q",  ACT_QUIT,  0 },
};

static const vt100_menu_item_t color_items[] = {
    { "Red",     NULL,  ACT_COLOR_RED,  0 },
    { "Green",   NULL,  ACT_COLOR_GRN,  0 },
    { "Blue",    NULL,  ACT_COLOR_BLU,  0 },
    { NULL,      NULL,  0,              MENU_ITEM_SEPARATOR },
    { "Reset",   NULL,  ACT_COLOR_RST,  0 },
};

static const vt100_menu_item_t help_items[] = {
    { "Help",  "F10",  ACT_HELP, 0 },
};

// ============================================================================
// STEP 3: Define top-level menus
// ============================================================================
// Each menu: { tab_label, items_array, item_count }

static const vt100_menu_def_t demo_menus[] = {
    { "File",   file_items,   3 },
    { "Color",  color_items,  5 },
    { "Help",   help_items,   1 },
};
#define DEMO_MENU_COUNT 3

// ============================================================================
// STEP 4: App state
// ============================================================================
// A real app (editor, viewer, scope) would have its own state struct.
// This demo just tracks a few simple things.

static struct {
    int  screen_rows;
    int  screen_cols;
    bool running;
    const char* color_esc;   /* current text colour SGR sequence */
    const char* message;     /* status line text */
} demo;

// ============================================================================
// STEP 5: I/O helpers
// ============================================================================
// The menu framework is I/O-agnostic.  Provide thin wrappers that route
// to your app's terminal I/O.  For Bus Pirate fullscreen apps, this means
// the USB FIFO (rx_fifo / tx_fifo), NOT stdio printf.

static void demo_write_str(const char* s) {
    tx_fifo_write(s, strlen(s));
}

static void demo_write_buf(const void* buf, int len) {
    tx_fifo_write((const char*)buf, (uint32_t)len);
}

// ============================================================================
// STEP 6: Key decoder — uses shared vt100_keys library
// ============================================================================
// The vt100_keys library handles all escape sequence decoding and pushback.
// We just provide thin I/O callbacks that route to the USB FIFO.

static int demo_read_blocking(char* c) {
    rx_fifo_get_blocking(c);
    return 1;
}

static int demo_read_try(char* c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

static vt100_key_state_t demo_keys;

// ============================================================================
// STEP 7: Menu I/O callbacks
// ============================================================================
// These thin wrappers adapt your app's I/O to the menu framework's
// function-pointer signatures.

static int demo_menu_read_key(void) {
    return vt100_key_read(&demo_keys);
}

static int demo_menu_write(int fd, const void* buf, int count) {
    (void)fd;
    tx_fifo_write((const char*)buf, (uint32_t)count);
    return count;
}

// ============================================================================
// STEP 8: Repaint callback
// ============================================================================
// Called by the menu framework when switching between dropdowns (left/right)
// so the area under the old dropdown is restored with actual content instead
// of blank spaces.  Without this, you get the "cascade blanking" effect.

static void demo_refresh_screen(void);  /* forward declaration */

static void demo_menu_repaint(void) {
    demo_refresh_screen();
}

// ============================================================================
// STEP 9: Screen refresh
// ============================================================================
// Draws the full app screen.  Row 1 is reserved for the menu bar (drawn
// separately), so content starts at row 2.
//
// A real app would render its data here (hex view, text buffer, scope trace).
// This demo just shows some informational text.

static void demo_refresh_screen(void) {
    char buf[256];
    int n;

    /* Hide cursor during rendering */
    demo_write_str("\x1b[?25l");

    /* Row 2: title */
    demo_write_str("\x1b[2;1H\x1b[0m\x1b[K");
    demo_write_str(demo.color_esc);
    demo_write_str("  VT100 Menu Demo  ");
    demo_write_str("\x1b[0m");

    /* Row 3: blank separator */
    demo_write_str("\x1b[3;1H\x1b[K");

    /* Row 4-8: instructions */
    demo_write_str("\x1b[4;1H\x1b[K  This is a minimal fullscreen app demonstrating the");
    demo_write_str("\x1b[5;1H\x1b[K  vt100_menu framework for Bus Pirate firmware.");
    demo_write_str("\x1b[6;1H\x1b[K");
    demo_write_str("\x1b[7;1H\x1b[K  Controls:");
    demo_write_str("\x1b[8;1H\x1b[K    F10     Open/close the menu bar");
    demo_write_str("\x1b[9;1H\x1b[K    Arrows  Navigate menus");
    demo_write_str("\x1b[10;1H\x1b[K    Enter   Select a menu item");
    demo_write_str("\x1b[11;1H\x1b[K    Escape  Close menu");
    demo_write_str("\x1b[12;1H\x1b[K    Ctrl-Q  Quit (works even while menu is open!)");
    demo_write_str("\x1b[13;1H\x1b[K");
    demo_write_str("\x1b[14;1H\x1b[K  The menu framework lives in:");
    demo_write_str("\x1b[15;1H\x1b[K    src/lib/vt100_menu/vt100_menu.h  (API)");
    demo_write_str("\x1b[16;1H\x1b[K    src/lib/vt100_menu/vt100_menu.c  (implementation)");
    demo_write_str("\x1b[17;1H\x1b[K");
    demo_write_str("\x1b[18;1H\x1b[K  Copy this file as a starting point for new apps.");

    /* Clear remaining rows */
    for (int r = 19; r < demo.screen_rows; r++) {
        n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[K", r);
        demo_write_buf(buf, n);
    }

    /* Status bar on the last row */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;1H\x1b[0;30;47m %-*s\x1b[0m",
                 demo.screen_rows, demo.screen_cols - 1,
                 demo.message ? demo.message : "");
    demo_write_buf(buf, n);

    /* Show cursor (text apps show it; hex editors hide it) */
    demo_write_str("\x1b[?25h");
}

// ============================================================================
// STEP 10: Action dispatch
// ============================================================================
// Route the action_id returned by vt100_menu_run() to your app's functions.
// This is the same pattern as editor_process_command / keypress handlers.

static void demo_dispatch(int action) {
    switch (action) {
    case ACT_QUIT:
        demo.running = false;
        break;
    case ACT_ABOUT:
        demo.message = "menu_demo: VT100 menu framework reference implementation";
        break;
    case ACT_HELP:
        demo.message = "F10=Menu | Arrows=Navigate | Enter=Select | ^Q=Quit";
        break;
    case ACT_COLOR_RED:
        demo.color_esc = "\x1b[1;37;41m";
        demo.message = "Color: Red";
        break;
    case ACT_COLOR_GRN:
        demo.color_esc = "\x1b[1;37;42m";
        demo.message = "Color: Green";
        break;
    case ACT_COLOR_BLU:
        demo.color_esc = "\x1b[1;37;44m";
        demo.message = "Color: Blue";
        break;
    case ACT_COLOR_RST:
        demo.color_esc = "\x1b[1;37;46m";
        demo.message = "Color: Reset (cyan)";
        break;
    }
}

// ============================================================================
// STEP 11: Keypress handler
// ============================================================================
// Process a single keypress in the app's normal (non-menu) mode.

static void demo_process_key(int key) {
    switch (key) {
    case VT100_KEY_CTRL_Q:
        demo.running = false;
        break;
    default:
        /* Ignore unrecognised keys in this simple demo */
        break;
    }
}

// ============================================================================
// STEP 12: Command handler (entry point)
// ============================================================================
// This function is called when the user types "menu_demo" at the prompt.
// It follows the same pattern as hexedit_handler / edit_handler:
//
//   1. Check help flag
//   2. Pause toolbars
//   3. Switch to alt screen
//   4. Drain stale rx bytes
//   5. Init menu + run main loop
//   6. Restore screen + toolbars

void menu_demo_handler(struct command_result* res) {

    /* Check help flag */
    if (bp_cmd_help_check(&menu_demo_def, res->help_flag)) {
        return;
    }

    /* Pause Core1 toolbar updates to avoid VT100 interleaving */
    toolbar_draw_prepare();

    /* Switch to alternate screen buffer */
    printf("\x1b[?1049h");  /* smcup — enter alt screen */
    printf("\x1b[r");       /* reset scroll region to full screen */
    printf("\x1b[2J");      /* clear alt screen */
    printf("\x1b[H");       /* cursor home */

    /* Drain any stale bytes from the rx FIFO */
    { char drain; while (rx_fifo_try_get(&drain)) {} }

    /* Init app state */
    demo.screen_rows  = system_config.terminal_ansi_rows;
    demo.screen_cols  = system_config.terminal_ansi_columns;
    demo.running      = true;
    demo.color_esc    = "\x1b[1;37;46m";  /* cyan background */
    demo.message      = "F10=Menu | Ctrl-Q=Quit";
    vt100_key_init(&demo_keys, demo_read_blocking, demo_read_try);

    /* ----------------------------------------------------------------
     * Initialise the menu system
     * ---------------------------------------------------------------- */
    vt100_menu_state_t menu_state;
    vt100_menu_init(&menu_state, demo_menus, DEMO_MENU_COUNT,
        1,  /* bar_row: row 1 (top of screen) */
        (uint8_t)demo.screen_cols,
        (uint8_t)demo.screen_rows,
        demo_menu_read_key,
        demo_menu_write);

    /* Key codes: both menu defaults and our key decoder now use VT100_KEY_*
     * values from vt100_keys.h, so no overrides are needed. */

    /* Set repaint callback — eliminates cascade blanking when switching menus */
    menu_state.repaint   = demo_menu_repaint;

    /* ----------------------------------------------------------------
     * Main loop
     * ----------------------------------------------------------------
     * Pattern:
     *   1. Wait for tx to drain (avoids VT100 interleaving)
     *   2. Refresh screen content
     *   3. Draw passive menu bar ("F10=Menu" hint)
     *   4. Read one keypress
     *   5. If F-key trigger, run the menu and dispatch result
     *   6. Otherwise, process keypress normally
     * ---------------------------------------------------------------- */
    while (demo.running) {
        tx_fifo_wait_drain();
        demo_refresh_screen();

        /* Draw passive menu bar */
        vt100_menu_draw_bar(&menu_state);

        /* Process one keypress */
        int key = vt100_key_read(&demo_keys);

        if (vt100_menu_check_trigger(&menu_state, key)) {
            /* Run the blocking menu interaction loop */
            int action = vt100_menu_run(&menu_state);

            if (action > 0) {
                /* User selected a menu item — dispatch it */
                demo_dispatch(action);
            } else if (action == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
                /* Key the menu didn't consume — push it back so the
                 * next demo_read_key() returns it immediately.
                 * This is how Ctrl-Q works while the menu is open. */
                vt100_key_unget(&demo_keys, menu_state.unhandled_key);
            }
            /* else: MENU_RESULT_CANCEL or MENU_RESULT_REDRAW — just redraw */

            /* Clear screen to force full repaint on next iteration */
            demo_write_str("\x1b[0m\x1b[H\x1b[2J");
        } else {
            demo_process_key(key);
        }
    }

    /* ----------------------------------------------------------------
     * Cleanup: restore terminal state
     * ---------------------------------------------------------------- */

    /* Drain any stale rx bytes */
    { char drain; while (rx_fifo_try_get(&drain)) {} }

    /* Restore main screen buffer */
    printf("\x1b[?1049l");  /* rmcup — leave alt screen */

    /* Re-apply the toolbar scroll region and position cursor */
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();  /* shows cursor, un-pauses Core1 */
}
