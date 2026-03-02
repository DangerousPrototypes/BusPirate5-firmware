// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file mines.c
 * @brief Mine Sweep — uncover a grid avoiding mines for Bus Pirate.
 *
 * Controls: WASD/arrows to move cursor, SPACE/ENTER to reveal,
 *           f to flag, r to restart, q to quit.
 * Default grid: 10x10 with 10 mines (scales to terminal size).
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
#include "mines.h"

#define MINES_MAX_ROWS 16
#define MINES_MAX_COLS 16
#define MINES_DEFAULT_ROWS 10
#define MINES_DEFAULT_COLS 10
#define MINES_DEFAULT_COUNT 10

static const char* const usage[] = {
    "mines",
    "Launch Mine Sweep:%s mines",
    "",
    "Uncover the grid without hitting a mine!",
    "WASD/arrows=move  SPACE=reveal  f=flag  r=restart  q=quit",
};

const bp_command_def_t mines_def = {
    .name = "mines",
    .description = T_HELP_MINES,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Cell state
// ---------------------------------------------------------------------------
// Bits: [mine][revealed][flagged][count 0-8]
#define CELL_MINE     0x80
#define CELL_REVEALED 0x40
#define CELL_FLAGGED  0x20
#define CELL_COUNT    0x0F

typedef struct {
    uint8_t grid[MINES_MAX_ROWS][MINES_MAX_COLS];
    int g_rows;
    int g_cols;
    int g_mines;
    int cur_r;
    int cur_c;
    bool first_click;
} mines_state_t;

static void count_adjacent(mines_state_t* gs) {
    for (int r = 0; r < gs->g_rows; r++) {
        for (int c = 0; c < gs->g_cols; c++) {
            if (gs->grid[r][c] & CELL_MINE) continue;
            int n = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < gs->g_rows && nc >= 0 && nc < gs->g_cols) {
                        if (gs->grid[nr][nc] & CELL_MINE) n++;
                    }
                }
            }
            gs->grid[r][c] = (gs->grid[r][c] & 0xF0) | (n & CELL_COUNT);
        }
    }
}

static void place_mines(mines_state_t* gs, int safe_r, int safe_c) {
    int placed = 0;
    while (placed < gs->g_mines) {
        int r = game_rng_next() % gs->g_rows;
        int c = game_rng_next() % gs->g_cols;
        // Don't place on safe cell or already mined
        if (r == safe_r && c == safe_c) continue;
        // Also keep 1-cell radius safe around first click
        if (r >= safe_r - 1 && r <= safe_r + 1 &&
            c >= safe_c - 1 && c <= safe_c + 1) continue;
        if (gs->grid[r][c] & CELL_MINE) continue;
        gs->grid[r][c] |= CELL_MINE;
        placed++;
    }
    count_adjacent(gs);
}

static void reveal_cell(mines_state_t* gs, int r, int c) {
    if (r < 0 || r >= gs->g_rows || c < 0 || c >= gs->g_cols) return;
    if (gs->grid[r][c] & (CELL_REVEALED | CELL_FLAGGED)) return;
    gs->grid[r][c] |= CELL_REVEALED;
    // If count is 0 (no adjacent mines), flood-fill reveal neighbors
    if (!(gs->grid[r][c] & CELL_MINE) && (gs->grid[r][c] & CELL_COUNT) == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                reveal_cell(gs, r + dr, c + dc);
            }
        }
    }
}

static bool check_win(mines_state_t* gs) {
    for (int r = 0; r < gs->g_rows; r++) {
        for (int c = 0; c < gs->g_cols; c++) {
            if (!(gs->grid[r][c] & CELL_MINE) && !(gs->grid[r][c] & CELL_REVEALED)) return false;
        }
    }
    return true;
}

static int count_flags(mines_state_t* gs) {
    int n = 0;
    for (int r = 0; r < gs->g_rows; r++)
        for (int c = 0; c < gs->g_cols; c++)
            if (gs->grid[r][c] & CELL_FLAGGED) n++;
    return n;
}

// ---------------------------------------------------------------------------
// Number colors
// ---------------------------------------------------------------------------
static const char* num_color(int n) {
    if (!system_config.terminal_ansi_color) return "";
    switch (n) {
        case 1: return "\x1b[34m";   // blue
        case 2: return "\x1b[32m";   // green
        case 3: return "\x1b[31m";   // red
        case 4: return "\x1b[35m";   // magenta
        case 5: return "\x1b[33m";   // yellow
        case 6: return "\x1b[36m";   // cyan
        case 7: return "\x1b[37m";   // white
        case 8: return "\x1b[90m";   // grey
        default: return "";
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void draw_grid(mines_state_t* gs, int start_row, int start_col, bool show_mines) {
    const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";

    for (int r = 0; r < gs->g_rows; r++) {
        ui_term_cursor_position(start_row + r, start_col);
        for (int c = 0; c < gs->g_cols; c++) {
            bool is_cursor = (r == gs->cur_r && c == gs->cur_c);
            uint8_t cell = gs->grid[r][c];

            // Cursor highlight
            if (is_cursor) printf("\x1b[7m");

            if ((cell & CELL_REVEALED) || (show_mines && (cell & CELL_MINE))) {
                if (cell & CELL_MINE) {
                    if (system_config.terminal_ansi_color)
                        printf("\x1b[31m*%s", rst);
                    else
                        printf("*");
                } else {
                    int cnt = cell & CELL_COUNT;
                    if (cnt == 0) {
                        printf(" ");
                    } else {
                        printf("%s%d%s", num_color(cnt), cnt, rst);
                    }
                }
            } else if (cell & CELL_FLAGGED) {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[33mF%s", rst);
                else
                    printf("F");
            } else {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[47;30m\xc2\xb7%s", rst); // grey dot on white
                else
                    printf(".");
            }

            if (is_cursor) printf("\x1b[27m"); // un-reverse
            printf(" "); // spacing between cells
        }
    }
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void mines_handler(struct command_result* res) {
    if (bp_cmd_help_check(&mines_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;
    if (term_rows < 14 || term_cols < 30) {
        printf("Terminal too small (need at least 30x14)\r\n");
        return;
    }

    game_screen_enter(0);

    game_rng_seed();

    mines_state_t gs = {0};

    gs.g_rows = MINES_DEFAULT_ROWS;
    gs.g_cols = MINES_DEFAULT_COLS;
    gs.g_mines = MINES_DEFAULT_COUNT;

    // Scale down if terminal is small
    int max_r = term_rows - 5;
    int max_c = (term_cols - 4) / 2; // each cell = 2 chars
    if (gs.g_rows > max_r) gs.g_rows = max_r;
    if (gs.g_cols > max_c) gs.g_cols = max_c;
    if (gs.g_rows < 5) gs.g_rows = 5;
    if (gs.g_cols < 5) gs.g_cols = 5;

    bool quit = false;

    while (!quit) {
        memset(gs.grid, 0, sizeof(gs.grid));
        gs.cur_r = gs.g_rows / 2;
        gs.cur_c = gs.g_cols / 2;
        gs.first_click = true;

        int board_h = gs.g_rows;
        int board_w = gs.g_cols * 2;
        int start_row = (term_rows - board_h - 3) / 2 + 1;
        int start_col = (term_cols - board_w) / 2;
        if (start_row < 3) start_row = 3;
        if (start_col < 1) start_col = 1;

        bool game_over = false;
        bool won_game = false;
        const char* status_msg = "Move cursor, SPACE=reveal, f=flag";

        printf("\x1b[2J");
        while (!game_over && !quit) {
            tx_fifo_wait_drain();

            // Title (overwrite in place)
            ui_term_cursor_position(start_row - 2, start_col);
            int flags = count_flags(&gs);
            printf("\x1b[1mMine Sweep\x1b[0m  Mines:%d  Flags:%d\x1b[K",
                   gs.g_mines, flags);

            draw_grid(&gs, start_row, start_col, false);

            // Status line
            ui_term_cursor_position(start_row + board_h + 1, start_col);
            printf("%s\x1b[K", status_msg);
            ui_term_cursor_position(start_row + board_h + 2, start_col);
            printf("WASD/arrows f=flag SPACE=dig r=new q=quit");

            // Wait for input
            bool got = false;
            while (!got && !quit) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                switch (s2) {
                                    case 'A': c = 'w'; break;
                                    case 'B': c = 's'; break;
                                    case 'C': c = 'd'; break;
                                    case 'D': c = 'a'; break;
                                    default: continue;
                                }
                            } else continue;
                        } else continue;
                    }

                    switch (c) {
                        case 'w': case 'W':
                            if (gs.cur_r > 0) gs.cur_r--;
                            got = true;
                            break;
                        case 's': case 'S':
                            if (gs.cur_r < gs.g_rows - 1) gs.cur_r++;
                            got = true;
                            break;
                        case 'a': case 'A':
                            if (gs.cur_c > 0) gs.cur_c--;
                            got = true;
                            break;
                        case 'd': case 'D':
                            if (gs.cur_c < gs.g_cols - 1) gs.cur_c++;
                            got = true;
                            break;
                        case 'f': case 'F':
                            if (!(gs.grid[gs.cur_r][gs.cur_c] & CELL_REVEALED)) {
                                gs.grid[gs.cur_r][gs.cur_c] ^= CELL_FLAGGED;
                            }
                            got = true;
                            break;
                        case ' ':
                        case '\r':
                        case '\n':
                            if (gs.grid[gs.cur_r][gs.cur_c] & CELL_FLAGGED) {
                                status_msg = "Unflag first! (press f)";
                                got = true;
                                break;
                            }
                            if (gs.grid[gs.cur_r][gs.cur_c] & CELL_REVEALED) {
                                got = true;
                                break;
                            }
                            if (gs.first_click) {
                                place_mines(&gs, gs.cur_r, gs.cur_c);
                                gs.first_click = false;
                            }
                            reveal_cell(&gs, gs.cur_r, gs.cur_c);
                            if (gs.grid[gs.cur_r][gs.cur_c] & CELL_MINE) {
                                // BOOM
                                game_over = true;
                                won_game = false;
                            } else if (check_win(&gs)) {
                                game_over = true;
                                won_game = true;
                            }
                            status_msg = "Move cursor, SPACE=reveal, f=flag";
                            got = true;
                            break;
                        case 'r': case 'R':
                            game_over = true;
                            got = true;
                            break;
                        case 'q': case 'Q':
                            quit = true;
                            break;
                    }
                }
                busy_wait_ms(5);
            }
        }

        if (!quit && (won_game || !gs.first_click)) {
            // Show result
            tx_fifo_wait_drain();
            printf("\x1b[2J");
            ui_term_cursor_position(start_row - 2, start_col);
            printf("\x1b[1mMine Sweep\x1b[0m");

            draw_grid(&gs, start_row, start_col, true); // reveal mines

            ui_term_cursor_position(start_row + board_h + 1, start_col);
            if (won_game) {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;32mYou win! All clear!\x1b[0m");
                else
                    printf("You win! All clear!");
            } else {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;31mBOOM! Hit a mine!\x1b[0m");
                else
                    printf("BOOM! Hit a mine!");
            }
            ui_term_cursor_position(start_row + board_h + 2, start_col);
            printf("r=play again  q=quit");

            bool decided = false;
            while (!decided) {
                char ch;
                if (rx_fifo_try_get(&ch)) {
                    if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                    if (ch == 'r' || ch == 'R') { decided = true; }
                }
                busy_wait_ms(10);
            }
        }
    }

    game_screen_exit();
}
