// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file tictactoe.c
 * @brief Tic-Tac-Toe — 3x3 grid vs CPU with minimax AI for Bus Pirate.
 *
 * Controls: 1-9 (numpad layout) to place X, q to quit, r to restart.
 * The CPU plays O using a full minimax search — unbeatable.
 *
 *  7 | 8 | 9
 * ---+---+---
 *  4 | 5 | 6
 * ---+---+---
 *  1 | 2 | 3
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
#include "tictactoe.h"

static const char* const usage[] = {
    "ttt",
    "Launch Tic-Tac-Toe:%s ttt",
    "",
    "You are X, CPU is O. CPU uses minimax (unbeatable).",
    "1-9=place (numpad layout)  r=restart  q=quit",
};

const bp_command_def_t tictactoe_def = {
    .name = "ttt",
    .description = T_HELP_TTT,
    .usage = usage,
    .usage_count = count_of(usage),
};

// Board: 0=empty, 'X', 'O'
typedef struct {
    char board[9];
} ttt_state_t;

// Numpad mapping: key 1-9 → board index (1=bottom-left, 9=top-right)
static const int key_map[10] = {-1, 6, 7, 8, 3, 4, 5, 0, 1, 2};

static void init_board(ttt_state_t* gs) {
    memset(gs->board, 0, sizeof(gs->board));
}

static char check_winner(ttt_state_t* gs) {
    static const int lines[8][3] = {
        {0,1,2},{3,4,5},{6,7,8}, // rows
        {0,3,6},{1,4,7},{2,5,8}, // cols
        {0,4,8},{2,4,6}          // diags
    };
    for (int i = 0; i < 8; i++) {
        char a = gs->board[lines[i][0]];
        if (a && a == gs->board[lines[i][1]] && a == gs->board[lines[i][2]]) return a;
    }
    return 0;
}

static bool board_full(ttt_state_t* gs) {
    for (int i = 0; i < 9; i++) if (!gs->board[i]) return false;
    return true;
}

// Minimax: returns +10 for O win, -10 for X win, 0 for draw
static int minimax(ttt_state_t* gs, bool is_o, int depth) {
    char w = check_winner(gs);
    if (w == 'O') return 10 - depth;
    if (w == 'X') return depth - 10;
    if (board_full(gs)) return 0;

    int best = is_o ? -100 : 100;
    for (int i = 0; i < 9; i++) {
        if (gs->board[i]) continue;
        gs->board[i] = is_o ? 'O' : 'X';
        int val = minimax(gs, !is_o, depth + 1);
        gs->board[i] = 0;
        if (is_o) { if (val > best) best = val; }
        else      { if (val < best) best = val; }
    }
    return best;
}

static int cpu_move(ttt_state_t* gs) {
    int best_score = -100;
    int best_idx = -1;
    for (int i = 0; i < 9; i++) {
        if (gs->board[i]) continue;
        gs->board[i] = 'O';
        int val = minimax(gs, false, 0);
        gs->board[i] = 0;
        if (val > best_score) {
            best_score = val;
            best_idx = i;
        }
    }
    return best_idx;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void draw_board_at(ttt_state_t* gs, int start_row, int start_col) {
    const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";

    for (int r = 0; r < 3; r++) {
        ui_term_cursor_position(start_row + r * 2, start_col);
        for (int c = 0; c < 3; c++) {
            if (c > 0) printf("|");
            char ch = gs->board[r * 3 + c];
            if (ch == 'X') {
                if (system_config.terminal_ansi_color) printf(" %sX%s ", "\x1b[1;32m", rst);
                else printf(" X ");
            } else if (ch == 'O') {
                if (system_config.terminal_ansi_color) printf(" %sO%s ", "\x1b[1;31m", rst);
                else printf(" O ");
            } else {
                // Show position number hint
                int num = 0;
                for (int k = 1; k <= 9; k++) {
                    if (key_map[k] == r * 3 + c) { num = k; break; }
                }
                if (system_config.terminal_ansi_color) printf(" %s%d%s ", "\x1b[2;37m", num, rst);
                else printf(" %d ", num);
            }
        }
        if (r < 2) {
            ui_term_cursor_position(start_row + r * 2 + 1, start_col);
            printf("---+---+---");
        }
    }
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void tictactoe_handler(struct command_result* res) {
    if (bp_cmd_help_check(&tictactoe_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;

    game_screen_enter(0);

    ttt_state_t gs = {0};

    int start_row = (term_rows - 5) / 2;
    int start_col = (term_cols - 11) / 2;
    if (start_row < 3) start_row = 3;
    if (start_col < 1) start_col = 1;

    bool quit = false;

    while (!quit) {
        init_board(&gs);
        bool game_active = true;
        const char* status = "Your turn (X). Press 1-9.";

        printf("\x1b[2J");
        while (game_active && !quit) {
            tx_fifo_wait_drain();

            ui_term_cursor_position(start_row - 2, start_col);
            printf("\x1b[1mTic-Tac-Toe\x1b[0m  You=\x1b[1;32mX\x1b[0m  CPU=\x1b[1;31mO\x1b[0m");

            draw_board_at(&gs, start_row, start_col);

            ui_term_cursor_position(start_row + 6, start_col);
            printf("%s\x1b[K", status);
            ui_term_cursor_position(start_row + 7, start_col);
            printf("1-9=place  r=restart  q=quit");

            // Wait for valid input
            bool got_move = false;
            while (!got_move && !quit) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; break; }
                    if (c == 'r' || c == 'R') { game_active = false; break; }
                    if (c >= '1' && c <= '9') {
                        int idx = key_map[c - '0'];
                        if (gs.board[idx] == 0) {
                            gs.board[idx] = 'X';
                            got_move = true;
                        } else {
                            status = "Spot taken! Try another.";
                            // Redraw with updated status
                            tx_fifo_wait_drain();
                            ui_term_cursor_position(start_row + 6, start_col);
                            ui_term_erase_line();
                            printf("%s", status);
                        }
                    }
                }
                busy_wait_ms(10);
            }
            if (quit || !game_active) break;

            // Check if player won or draw
            char w = check_winner(&gs);
            if (w == 'X') {
                status = "You win! (Impressive!) r=again q=quit";
                // Show board, wait
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                ui_term_cursor_position(start_row - 2, start_col);
                printf("\x1b[1mTic-Tac-Toe\x1b[0m  You=\x1b[1;32mX\x1b[0m  CPU=\x1b[1;31mO\x1b[0m");
                draw_board_at(&gs, start_row, start_col);
                ui_term_cursor_position(start_row + 6, start_col);
                printf("\x1b[1;32m%s\x1b[0m", status);
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
                status = "Draw! r=again q=quit";
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                ui_term_cursor_position(start_row - 2, start_col);
                printf("\x1b[1mTic-Tac-Toe\x1b[0m  You=\x1b[1;32mX\x1b[0m  CPU=\x1b[1;31mO\x1b[0m");
                draw_board_at(&gs, start_row, start_col);
                ui_term_cursor_position(start_row + 6, start_col);
                printf("\x1b[1;33m%s\x1b[0m", status);
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

            // CPU move
            int cm = cpu_move(&gs);
            if (cm >= 0) gs.board[cm] = 'O';

            w = check_winner(&gs);
            if (w == 'O') {
                status = "CPU wins! r=again q=quit";
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                ui_term_cursor_position(start_row - 2, start_col);
                printf("\x1b[1mTic-Tac-Toe\x1b[0m  You=\x1b[1;32mX\x1b[0m  CPU=\x1b[1;31mO\x1b[0m");
                draw_board_at(&gs, start_row, start_col);
                ui_term_cursor_position(start_row + 6, start_col);
                printf("\x1b[1;31m%s\x1b[0m", status);
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
                status = "Draw! r=again q=quit";
                tx_fifo_wait_drain();
                printf("\x1b[2J");
                ui_term_cursor_position(start_row - 2, start_col);
                printf("\x1b[1mTic-Tac-Toe\x1b[0m  You=\x1b[1;32mX\x1b[0m  CPU=\x1b[1;31mO\x1b[0m");
                draw_board_at(&gs, start_row, start_col);
                ui_term_cursor_position(start_row + 6, start_col);
                printf("\x1b[1;33m%s\x1b[0m", status);
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

            status = "Your turn (X). Press 1-9.";
        }
    }

    game_screen_exit();
}
