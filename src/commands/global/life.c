// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file life.c
 * @brief Conway's Game of Life — fullscreen cellular automaton for Bus Pirate.
 *
 * A compact implementation that runs in the VT100 terminal using ANSI escape
 * codes.  Two bit-packed grids are double-buffered so the update is flicker-free.
 *
 * Controls:
 *   SPACE  — pause / resume
 *   r      — randomize the grid
 *   c      — clear the grid
 *   +/-    — faster / slower
 *   q      — quit
 *
 * Registration (commands.c):
 *   { .command="life", .allow_hiz=true, .func=&life_handler,
 *     .def=&life_def, .category=CMD_CAT_TOOLS }
 */

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
#include "lib/bp_args/bp_cmd.h"
#include "game_engine.h"
#include "life.h"

// ---------------------------------------------------------------------------
// Grid limits — terminal size minus a status row at the bottom.
// Each cell is two characters wide ("██" or "  ") for a square aspect ratio.
// ---------------------------------------------------------------------------
#define LIFE_MAX_ROWS 50
#define LIFE_MAX_COLS 80 // in *cell* columns (each cell = 2 terminal columns)

// Bit-packed storage: one bit per cell, 8 cells per byte.
#define GRID_BYTES(cols) (((cols) + 7) / 8)

// Two grids for double-buffering.
typedef struct {
    uint8_t grid_a[LIFE_MAX_ROWS][GRID_BYTES(LIFE_MAX_COLS)];
    uint8_t grid_b[LIFE_MAX_ROWS][GRID_BYTES(LIFE_MAX_COLS)];
    const char* CELL_ALIVE;
    const char* CELL_DEAD;
} life_state_t;

typedef uint8_t (*grid_t)[GRID_BYTES(LIFE_MAX_COLS)];

static inline bool cell_get(grid_t g, int r, int c) {
    return (g[r][c >> 3] >> (c & 7)) & 1;
}

static inline void cell_set(grid_t g, int r, int c) {
    g[r][c >> 3] |= (1 << (c & 7));
}

static inline void cell_clear_all(grid_t g, int rows, int cols) {
    int bytes_per_row = GRID_BYTES(cols);
    for (int r = 0; r < rows; r++) {
        memset(g[r], 0, bytes_per_row);
    }
}

// ---------------------------------------------------------------------------
// Command definition
// ---------------------------------------------------------------------------
static const char* const usage[] = {
    "life",
    "Launch Game of Life:%s life",
    "",
    "Conway's Game of Life cellular automaton.",
    "SPACE=pause  r=randomize  c=clear  +/-=speed  q=quit",
};

const bp_command_def_t life_def = {
    .name = "life",
    .description = T_HELP_LIFE,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Grid helpers
// ---------------------------------------------------------------------------
static void grid_randomize(grid_t g, int rows, int cols) {
    cell_clear_all(g, rows, cols);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if ((game_rng_next() & 3) == 0) { // ~25% density
                cell_set(g, r, c);
            }
        }
    }
}

static int count_neighbors(grid_t g, int r, int c, int rows, int cols) {
    int n = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) {
                continue;
            }
            int nr = r + dr;
            int nc = c + dc;
            // Wrap around (toroidal grid)
            if (nr < 0) nr = rows - 1;
            if (nr >= rows) nr = 0;
            if (nc < 0) nc = cols - 1;
            if (nc >= cols) nc = 0;
            if (cell_get(g, nr, nc)) {
                n++;
            }
        }
    }
    return n;
}

static void step(grid_t src, grid_t dst, int rows, int cols) {
    cell_clear_all(dst, rows, cols);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int n = count_neighbors(src, r, c, rows, cols);
            bool alive = cell_get(src, r, c);
            // Birth: dead cell with exactly 3 neighbors becomes alive.
            // Survival: live cell with 2 or 3 neighbors stays alive.
            if (n == 3 || (alive && n == 2)) {
                cell_set(dst, r, c);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Rendering — only redraws cells that changed, for speed.
// ---------------------------------------------------------------------------
static void render_full(life_state_t* gs, grid_t g, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        ui_term_cursor_position(r + 1, 1);
        for (int c = 0; c < cols; c++) {
            printf("%s", cell_get(g, r, c) ? gs->CELL_ALIVE : gs->CELL_DEAD);
        }
    }
}

static void render_diff(life_state_t* gs, grid_t prev, grid_t cur, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        // Quick row-level skip: if the entire row is unchanged, skip it.
        int bytes = GRID_BYTES(cols);
        bool row_changed = false;
        for (int b = 0; b < bytes; b++) {
            if (prev[r][b] != cur[r][b]) {
                row_changed = true;
                break;
            }
        }
        if (!row_changed) {
            continue;
        }
        for (int c = 0; c < cols; c++) {
            bool was = cell_get(prev, r, c);
            bool now = cell_get(cur, r, c);
            if (was != now) {
                ui_term_cursor_position(r + 1, c * 2 + 1);
                printf("%s", now ? gs->CELL_ALIVE : gs->CELL_DEAD);
            }
        }
    }
}

static void draw_status(int row, int cols_total, int gen, int pop, int delay_ms, bool paused) {
    ui_term_cursor_position(row, 1);
    printf("\x1b[7m"); // reverse video
    printf(" Gen:%06d Pop:%06d %dms%s  SPACE=pause r=reset +/-=speed q=quit",
           gen, pop, delay_ms, paused ? " [PAUSED]" : "");
    ui_term_erase_line();
    printf("\x1b[0m"); // reset
}

static int population(grid_t g, int rows, int cols) {
    int pop = 0;
    int bytes = GRID_BYTES(cols);
    for (int r = 0; r < rows; r++) {
        for (int b = 0; b < bytes; b++) {
            // Count set bits (Brian Kernighan's trick)
            uint8_t v = g[r][b];
            while (v) {
                v &= v - 1;
                pop++;
            }
        }
    }
    return pop;
}

// ---------------------------------------------------------------------------
// Main command handler
// ---------------------------------------------------------------------------
void life_handler(struct command_result* res) {

    if (bp_cmd_help_check(&life_def, res->help_flag)) {
        return;
    }

    life_state_t gs = {0};

    // Use block characters for color terminals, text for plain.
    if (system_config.terminal_ansi_color) {
        gs.CELL_ALIVE = "\x1b[32m\xe2\x96\x88\xe2\x96\x88\x1b[0m"; // green "██"
        gs.CELL_DEAD = "  ";
    } else {
        gs.CELL_ALIVE = "##";
        gs.CELL_DEAD = "  ";
    }

    // Calculate grid size from terminal dimensions.
    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;
    if (term_rows < 6 || term_cols < 20) {
        printf("Terminal too small (need at least 20x6)\r\n");
        return;
    }

    int rows = term_rows - 1; // reserve bottom row for status
    int cols = term_cols / 2; // each cell = 2 chars wide
    if (rows > LIFE_MAX_ROWS) rows = LIFE_MAX_ROWS;
    if (cols > LIFE_MAX_COLS) cols = LIFE_MAX_COLS;

    game_screen_enter(rows);

    // Seed PRNG and fill grid.
    game_rng_seed();
    grid_t cur = gs.grid_a;
    grid_t nxt = gs.grid_b;
    grid_randomize(cur, rows, cols);

    int generation = 0;
    int delay_ms = 100;
    bool paused = false;
    bool running = true;
    bool need_full_redraw = true;

    while (running) {
        tx_fifo_wait_drain();

        // Render.
        if (need_full_redraw) {
            printf("\x1b[2J"); // clear screen
            render_full(&gs, cur, rows, cols);
            need_full_redraw = false;
        }

        int pop = population(cur, rows, cols);
        draw_status(term_rows, term_cols, generation, pop, delay_ms, paused);

        // Wait for the tick interval, checking for input.
        absolute_time_t deadline = make_timeout_time_ms(delay_ms);
        while (!time_reached(deadline) || paused) {
            char c;
            if (rx_fifo_try_get(&c)) {
                switch (c) {
                    case 'q':
                    case 'Q':
                        running = false;
                        goto done;
                    case ' ':
                        paused = !paused;
                        draw_status(term_rows, term_cols, generation, pop, delay_ms, paused);
                        break;
                    case 'r':
                    case 'R':
                        game_rng_seed();
                        grid_randomize(cur, rows, cols);
                        generation = 0;
                        need_full_redraw = true;
                        paused = false;
                        break;
                    case '+':
                    case '=':
                        if (delay_ms > 10) delay_ms -= 10;
                        draw_status(term_rows, term_cols, generation, pop, delay_ms, paused);
                        break;
                    case '-':
                    case '_':
                        if (delay_ms < 1000) delay_ms += 10;
                        draw_status(term_rows, term_cols, generation, pop, delay_ms, paused);
                        break;
                    default:
                        break;
                }
                if (!running) break;
            }
            if (paused) {
                busy_wait_ms(10); // don't spin hard while paused
            }
        }

        if (!paused) {
            // Compute next generation.
            step(cur, nxt, rows, cols);
            render_diff(&gs, cur, nxt, rows, cols);
            generation++;

            // Swap buffers.
            grid_t tmp = cur;
            cur = nxt;
            nxt = tmp;
        }
    }

done:
    game_screen_exit();
}
