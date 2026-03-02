// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file drop4.c
 * @brief Drop Four — drop discs in columns vs CPU for Bus Pirate.
 *
 * 7 columns × 6 rows. You are X (yellow), CPU is O (red).
 * CPU uses simple heuristic AI: win > block > center > random.
 * Controls: 1-7 or arrows + enter = drop disc, q = quit.
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
#include "drop4.h"

#define C4_ROWS 6
#define C4_COLS 7

static const char* const usage[] = {
    "drop4",
    "Launch Drop Four:%s drop4",
    "",
    "Drop discs to get 4 in a row! You=X(yellow), CPU=O(red).",
    "1-7=drop in column  arrows+enter=select  r=restart  q=quit",
};

const bp_command_def_t drop4_def = {
    .name = "drop4",
    .description = T_HELP_DROP4,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Board state — 0=empty, 1=player (X), 2=CPU (O)
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t board[C4_ROWS][C4_COLS];
    int cursor_col;
} drop4_state_t;

static void init_board(drop4_state_t* gs) {
    memset(gs->board, 0, sizeof(gs->board));
    gs->cursor_col = C4_COLS / 2;
}

// Drop piece in column, returns row placed or -1 if full
static int drop(drop4_state_t* gs, int col, uint8_t piece) {
    for (int r = C4_ROWS - 1; r >= 0; r--) {
        if (!gs->board[r][col]) {
            gs->board[r][col] = piece;
            return r;
        }
    }
    return -1;
}

static void undrop(drop4_state_t* gs, int col) {
    for (int r = 0; r < C4_ROWS; r++) {
        if (gs->board[r][col]) { gs->board[r][col] = 0; return; }
    }
}

static bool col_full(drop4_state_t* gs, int col) {
    return gs->board[0][col] != 0;
}

static bool board_full(drop4_state_t* gs) {
    for (int c = 0; c < C4_COLS; c++) if (!col_full(gs, c)) return false;
    return true;
}

// Check for 4-in-a-row of given piece
static bool check_win(drop4_state_t* gs, uint8_t piece) {
    // Horizontal
    for (int r = 0; r < C4_ROWS; r++)
        for (int c = 0; c <= C4_COLS - 4; c++)
            if (gs->board[r][c]==piece && gs->board[r][c+1]==piece &&
                gs->board[r][c+2]==piece && gs->board[r][c+3]==piece) return true;
    // Vertical
    for (int r = 0; r <= C4_ROWS - 4; r++)
        for (int c = 0; c < C4_COLS; c++)
            if (gs->board[r][c]==piece && gs->board[r+1][c]==piece &&
                gs->board[r+2][c]==piece && gs->board[r+3][c]==piece) return true;
    // Diagonal down-right
    for (int r = 0; r <= C4_ROWS - 4; r++)
        for (int c = 0; c <= C4_COLS - 4; c++)
            if (gs->board[r][c]==piece && gs->board[r+1][c+1]==piece &&
                gs->board[r+2][c+2]==piece && gs->board[r+3][c+3]==piece) return true;
    // Diagonal down-left
    for (int r = 0; r <= C4_ROWS - 4; r++)
        for (int c = 3; c < C4_COLS; c++)
            if (gs->board[r][c]==piece && gs->board[r+1][c-1]==piece &&
                gs->board[r+2][c-2]==piece && gs->board[r+3][c-3]==piece) return true;
    return false;
}

// Simple AI: win > block > center > random
static int cpu_choose(drop4_state_t* gs) {
    // 1. Can CPU win?
    for (int c = 0; c < C4_COLS; c++) {
        if (col_full(gs, c)) continue;
        drop(gs, c, 2);
        if (check_win(gs, 2)) { undrop(gs, c); return c; }
        undrop(gs, c);
    }
    // 2. Must block player?
    for (int c = 0; c < C4_COLS; c++) {
        if (col_full(gs, c)) continue;
        drop(gs, c, 1);
        if (check_win(gs, 1)) { undrop(gs, c); return c; }
        undrop(gs, c);
    }
    // 3. Prefer center
    if (!col_full(gs, 3)) return 3;
    // 4. Random valid column
    int tries = 0;
    while (tries < 100) {
        int c = game_rng_next() % C4_COLS;
        if (!col_full(gs, c)) return c;
        tries++;
    }
    // Fallback
    for (int c = 0; c < C4_COLS; c++) if (!col_full(gs, c)) return c;
    return 0;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void draw_board(drop4_state_t* gs, int sr, int sc) {
    const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";

    // Column numbers + cursor
    ui_term_cursor_position(sr, sc);
    for (int c = 0; c < C4_COLS; c++) {
        if (c == gs->cursor_col) {
            printf("\x1b[7m %d %s", c + 1, rst);
        } else {
            printf(" %d ", c + 1);
        }
        if (c < C4_COLS - 1) printf(" ");
    }

    // Board
    for (int r = 0; r < C4_ROWS; r++) {
        ui_term_cursor_position(sr + 1 + r, sc);
        for (int c = 0; c < C4_COLS; c++) {
            printf("|");
            if (gs->board[r][c] == 1) {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;33m X %s", rst);  // yellow
                else
                    printf(" X ");
            } else if (gs->board[r][c] == 2) {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;31m O %s", rst);  // red
                else
                    printf(" O ");
            } else {
                printf("   ");
            }
        }
        printf("|");
    }

    // Bottom
    ui_term_cursor_position(sr + 1 + C4_ROWS, sc);
    printf("+");
    for (int c = 0; c < C4_COLS; c++) printf("---+");
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void drop4_handler(struct command_result* res) {
    if (bp_cmd_help_check(&drop4_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;

    game_screen_enter(0);

    game_rng_seed();

    drop4_state_t gs = {0};

    int board_w = C4_COLS * 4 + 1;
    int sr = (term_rows - C4_ROWS - 4) / 2;
    int sc = (term_cols - board_w) / 2;
    if (sr < 3) sr = 3;
    if (sc < 1) sc = 1;

    bool quit = false;

    while (!quit) {
        init_board(&gs);
        bool game_active = true;
        const char* status = "Your turn (X). 1-7 or arrows+enter.";

        printf("\x1b[2J");
        while (game_active && !quit) {
            tx_fifo_wait_drain();

            ui_term_cursor_position(sr - 2, sc);
            printf("\x1b[1mDrop Four\x1b[0m  You=X  CPU=O");

            draw_board(&gs, sr, sc);

            ui_term_cursor_position(sr + C4_ROWS + 3, sc);
            printf("%s\x1b[K", status);
            ui_term_cursor_position(sr + C4_ROWS + 4, sc);
            printf("1-7/arrows+enter=drop  r=restart  q=quit");

            // Wait for player input
            bool got_move = false;
            while (!got_move && !quit) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; break; }
                    if (c == 'r' || c == 'R') { game_active = false; break; }

                    if (c == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                if (s2 == 'D' && gs.cursor_col > 0) gs.cursor_col--;
                                else if (s2 == 'C' && gs.cursor_col < C4_COLS - 1) gs.cursor_col++;
                                // Redraw cursor
                                tx_fifo_wait_drain();
                                ui_term_cursor_position(sr, sc);
                                const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";
                                for (int cc = 0; cc < C4_COLS; cc++) {
                                    if (cc == gs.cursor_col) printf("\x1b[7m %d %s", cc+1, rst);
                                    else printf(" %d ", cc+1);
                                    if (cc < C4_COLS-1) printf(" ");
                                }
                            }
                        }
                        continue;
                    }

                    int col = -1;
                    if (c >= '1' && c <= '7') col = c - '1';
                    else if (c == '\r' || c == '\n' || c == ' ') col = gs.cursor_col;

                    if (col >= 0 && col < C4_COLS && !col_full(&gs, col)) {
                        drop(&gs, col, 1);
                        got_move = true;
                    } else if (col >= 0) {
                        status = "Column full! Try another.";
                        tx_fifo_wait_drain();
                        ui_term_cursor_position(sr + C4_ROWS + 3, sc);
                        ui_term_erase_line();
                        printf("%s", status);
                    }
                }
                busy_wait_ms(10);
            }
            if (quit || !game_active) break;

            // Check player win
            if (check_win(&gs, 1)) {
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                ui_term_cursor_position(sr - 2, sc);
                printf("\x1b[1mDrop Four\x1b[0m");
                draw_board(&gs, sr, sc);
                ui_term_cursor_position(sr + C4_ROWS + 3, sc);
                printf("\x1b[1;32mYou win! r=again q=quit\x1b[0m");
                bool decided = false;
                while (!decided) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                        if (ch == 'r' || ch == 'R') { decided = true; }
                    }
                    busy_wait_ms(10);
                }
                game_active = false;
                continue;
            }
            if (board_full(&gs)) {
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                draw_board(&gs, sr, sc);
                ui_term_cursor_position(sr + C4_ROWS + 3, sc);
                printf("\x1b[1;33mDraw! r=again q=quit\x1b[0m");
                bool decided = false;
                while (!decided) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                        if (ch == 'r' || ch == 'R') { decided = true; }
                    }
                    busy_wait_ms(10);
                }
                game_active = false;
                continue;
            }

            // CPU turn
            int cpu_col = cpu_choose(&gs);
            drop(&gs, cpu_col, 2);

            if (check_win(&gs, 2)) {
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                ui_term_cursor_position(sr - 2, sc);
                printf("\x1b[1mDrop Four\x1b[0m");
                draw_board(&gs, sr, sc);
                ui_term_cursor_position(sr + C4_ROWS + 3, sc);
                printf("\x1b[1;31mCPU wins! r=again q=quit\x1b[0m");
                bool decided = false;
                while (!decided) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                        if (ch == 'r' || ch == 'R') { decided = true; }
                    }
                    busy_wait_ms(10);
                }
                game_active = false;
                continue;
            }

            status = "Your turn (X). 1-7 or arrows+enter.";
        }
    }

    game_screen_exit();
}
