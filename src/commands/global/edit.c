/*
 * edit.c — "edit" command handler for Bus Pirate
 *
 * Thin wrapper around the kilo editor (antirez/kilo, BSD-2-Clause).
 * Pauses the toolbar, switches to the VT100 alternate screen buffer,
 * runs the editor, then restores everything on exit.
 *
 * Usage:   edit filename.txt
 */

#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "fatfs/ff.h"
#include "pirate/file.h"
#include "pirate/mem.h"
#include "lib/kilo/kilo_compat.h"
#include "lib/bp_args/bp_cmd.h"

/* ---- Command definition ---- */

static const char* const usage[] = {
    "edit <filename>",
    "Open existing file:%s edit notes.txt",
    "Create new file:%s edit newfile.txt",
    "",
    "Keyboard shortcuts:",
    "  Ctrl-S  Save file",
    "  Ctrl-Q  Quit (press 3x to discard changes)",
    "  Ctrl-F  Find text in file",
};

static const bp_command_opt_t edit_opts[] = {
    { 0 },
};

static const bp_command_positional_t edit_positionals[] = {
    { .name = "filename", .hint = "file", .description = 0, .required = true },
    { 0 },
};

const bp_command_def_t edit_def = {
    .name         = "edit",
    .description  = 0,
    .actions      = NULL,
    .action_count = 0,
    .opts         = edit_opts,
    .positionals  = edit_positionals,
    .positional_count = 1,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

/* ---- Command handler ---- */

void edit_handler(struct command_result *res) {

    //check help flag
    if (bp_cmd_help_check(&edit_def, res->help_flag)) {
        return;
    }

    /* Parse filename from command line */
    char filename[64];
    if (!bp_file_get_name_positional(&edit_def, 1, filename, sizeof(filename))) {
        printf("Usage: edit <filename>\r\n");
        res->error = true;
        return;
    }

    /* Allocate the BIG_BUFFER (128KB) for the editor's arena allocator.
     * All of kilo's malloc/realloc/free go here — zero system heap usage. */
    uint8_t *arena = mem_alloc(BIG_BUFFER_SIZE, BP_BIG_BUFFER_EDITOR);
    if (!arena) {
        printf("Error: memory in use by another feature\r\n");
        res->error = true;
        return;
    }
    kilo_arena_init(arena, BIG_BUFFER_SIZE);

    /* Pause Core1 toolbar updates to avoid VT100 interleaving.
     * toolbar_draw_prepare() spins until any in-progress Core1 render
     * cycle finishes, then hides the cursor. */
    toolbar_draw_prepare();

    /* Switch to alternate screen buffer */
    printf("\x1b[?1049h");  /* smcup — enter alt screen */
    printf("\x1b[r");       /* reset scroll region to full screen */
    printf("\x1b[2J");      /* clear alt screen */
    printf("\x1b[H");       /* cursor home */

    /* Drain any stale bytes from the rx FIFO.
     * Linenoise / command parsing may leave trailing bytes that would
     * confuse kilo's keyboard handler. */
    { char drain; while (rx_fifo_try_get(&drain)) {} }

    /* Run the kilo editor.
     * exit() inside kilo is #define'd to longjmp(kilo_exit_jmpbuf, code).
     * setjmp returns 0 on first call, nonzero when exit() fires. */
    int exit_code = setjmp(kilo_exit_jmpbuf);
    if (exit_code == 0) {
        /* First call - enter the editor */
        kilo_run(filename);
        /* kilo_run never returns (infinite loop until exit()) */
    }
    /* If we get here, exit() was called:
     *   exit_code 255 = normal quit (Ctrl-Q, mapped from exit(0))
     *   exit_code 2   = out of arena memory
     *   exit_code 1   = error */

    /* Free all editor allocations (marks arena blocks free) */
    kilo_cleanup();

    /* Release the BIG_BUFFER back to the system */
    mem_free(arena);

    /* Drain any bytes left in the rx FIFO.
     * The final Ctrl-Q sequence or VT100 response bytes from the last
     * screen refresh can leak through and be picked up by linenoise
     * as a phantom command character. */
    { char drain; while (rx_fifo_try_get(&drain)) {} }

    /* Restore main screen buffer.
     * rmcup restores saved terminal state including the toolbar scroll region. */
    printf("\x1b[?1049l");  /* rmcup — leave alt screen */

    /* Re-apply the toolbar scroll region and position the cursor at
     * the bottom of the scroll region (where the prompt will appear).
     * Without the cursor positioning, rmcup may leave it at row 1.
     *
     * NOTE: do NOT call toolbar_redraw_all() here — it has its own
     * prepare/release lifecycle that conflicts with our outer
     * toolbar_draw_prepare().  Let Core1 repaint the toolbar
     * naturally after we call toolbar_draw_release(). */
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();  /* shows cursor, un-pauses Core1 */

    if (exit_code == 2) {
        printf("Editor: out of memory (%d KB limit). File NOT saved.\r\n",
               BIG_BUFFER_SIZE / 1024);
        printf("Tip: large files may exceed the editor's %d KB buffer.\r\n",
               BIG_BUFFER_SIZE / 1024);
    }

    res->error = (exit_code != 255);
}
