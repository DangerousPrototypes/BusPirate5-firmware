// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file game2048.c
 * @brief 2048 — slide numbered tiles on a 4x4 grid for Bus Pirate.
 *
 * Controls: WASD or arrow keys to slide, q to quit, r to restart.
 * Merge matching tiles to reach 2048!
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
#include "game2048.h"

#define GRID_SIZE 4

static const char* const usage[] = {
    "2048",
    "Launch 2048:%s 2048",
    "",
    "Slide tiles to merge matching numbers. Reach 2048 to win!",
    "WASD/arrows=slide  r=restart  q=quit",
};

const bp_command_def_t game2048_def = {
    .name = "2048",
    .description = T_HELP_2048,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Board state
// ---------------------------------------------------------------------------
typedef struct {
    uint16_t board[GRID_SIZE][GRID_SIZE];
    int score;
    bool won;
} g2048_state_t;

static int count_empty(g2048_state_t* gs) {
    int n = 0;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (gs->board[r][c] == 0) n++;
    return n;
}

static void add_random_tile(g2048_state_t* gs) {
    int empty = count_empty(gs);
    if (empty == 0) return;
    int idx = game_rng_next() % empty;
    int n = 0;
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (gs->board[r][c] == 0) {
                if (n == idx) {
                    gs->board[r][c] = (game_rng_next() % 10 == 0) ? 4 : 2;
                    return;
                }
                n++;
            }
        }
    }
}

static void init_board(g2048_state_t* gs) {
    memset(gs->board, 0, sizeof(gs->board));
    gs->score = 0;
    gs->won = false;
    add_random_tile(gs);
    add_random_tile(gs);
}

// Slide a single row left, merging. Returns true if anything moved.
static bool slide_row_left(g2048_state_t* gs, uint16_t row[GRID_SIZE]) {
    bool moved = false;
    // Compact non-zero to the left
    uint16_t tmp[GRID_SIZE] = {0};
    int pos = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        if (row[i]) tmp[pos++] = row[i];
    }
    // Merge adjacent equal
    for (int i = 0; i < GRID_SIZE - 1; i++) {
        if (tmp[i] && tmp[i] == tmp[i + 1]) {
            tmp[i] *= 2;
            gs->score += tmp[i];
            if (tmp[i] == 2048) gs->won = true;
            tmp[i + 1] = 0;
        }
    }
    // Compact again
    uint16_t out[GRID_SIZE] = {0};
    pos = 0;
    for (int i = 0; i < GRID_SIZE; i++) {
        if (tmp[i]) out[pos++] = tmp[i];
    }
    for (int i = 0; i < GRID_SIZE; i++) {
        if (row[i] != out[i]) moved = true;
        row[i] = out[i];
    }
    return moved;
}

static bool move_left(g2048_state_t* gs) {
    bool moved = false;
    for (int r = 0; r < GRID_SIZE; r++)
        if (slide_row_left(gs, gs->board[r])) moved = true;
    return moved;
}

static bool move_right(g2048_state_t* gs) {
    bool moved = false;
    // Reverse each row, slide left, reverse back
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int i = 0; i < GRID_SIZE / 2; i++) {
            uint16_t t = gs->board[r][i];
            gs->board[r][i] = gs->board[r][GRID_SIZE - 1 - i];
            gs->board[r][GRID_SIZE - 1 - i] = t;
        }
        if (slide_row_left(gs, gs->board[r])) moved = true;
        for (int i = 0; i < GRID_SIZE / 2; i++) {
            uint16_t t = gs->board[r][i];
            gs->board[r][i] = gs->board[r][GRID_SIZE - 1 - i];
            gs->board[r][GRID_SIZE - 1 - i] = t;
        }
    }
    return moved;
}

static bool move_up(g2048_state_t* gs) {
    bool moved = false;
    for (int c = 0; c < GRID_SIZE; c++) {
        uint16_t col[GRID_SIZE];
        for (int r = 0; r < GRID_SIZE; r++) col[r] = gs->board[r][c];
        if (slide_row_left(gs, col)) moved = true;
        for (int r = 0; r < GRID_SIZE; r++) gs->board[r][c] = col[r];
    }
    return moved;
}

static bool move_down(g2048_state_t* gs) {
    bool moved = false;
    for (int c = 0; c < GRID_SIZE; c++) {
        uint16_t col[GRID_SIZE];
        for (int r = 0; r < GRID_SIZE; r++) col[r] = gs->board[GRID_SIZE - 1 - r][c];
        if (slide_row_left(gs, col)) moved = true;
        for (int r = 0; r < GRID_SIZE; r++) gs->board[GRID_SIZE - 1 - r][c] = col[r];
    }
    return moved;
}

static bool can_move(g2048_state_t* gs) {
    if (count_empty(gs) > 0) return true;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE - 1; c++)
            if (gs->board[r][c] == gs->board[r][c + 1]) return true;
    for (int c = 0; c < GRID_SIZE; c++)
        for (int r = 0; r < GRID_SIZE - 1; r++)
            if (gs->board[r][c] == gs->board[r + 1][c]) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Tile colors (ANSI 256-color approximation by value)
// ---------------------------------------------------------------------------
static const char* tile_color(uint16_t val) {
    if (!system_config.terminal_ansi_color) return "";
    switch (val) {
        case 2:    return "\x1b[47;30m";    // white bg, black text
        case 4:    return "\x1b[43;30m";    // yellow bg
        case 8:    return "\x1b[41;37m";    // red bg, white text
        case 16:   return "\x1b[45;37m";    // magenta bg
        case 32:   return "\x1b[42;37m";    // green bg
        case 64:   return "\x1b[46;30m";    // cyan bg
        case 128:  return "\x1b[44;37m";    // blue bg
        case 256:  return "\x1b[43;37m";    // yellow bg, white text
        case 512:  return "\x1b[41;33m";    // red bg, yellow text
        case 1024: return "\x1b[45;33m";    // magenta bg, yellow
        case 2048: return "\x1b[42;33m";    // green bg, yellow
        default:   return "\x1b[100;37m";   // dark grey bg
    }
}

// ---------------------------------------------------------------------------
// Drawing — centered board
// ---------------------------------------------------------------------------
#define TILE_W 7 // chars per tile cell

static void draw_board(g2048_state_t* gs, int start_row, int start_col) {
    const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";

    for (int r = 0; r < GRID_SIZE; r++) {
        // Separator line
        ui_term_cursor_position(start_row + r * 2, start_col);
        for (int c = 0; c < GRID_SIZE; c++) {
            printf("+-------");
        }
        printf("+");

        // Value line
        ui_term_cursor_position(start_row + r * 2 + 1, start_col);
        for (int c = 0; c < GRID_SIZE; c++) {
            printf("|");
            if (gs->board[r][c]) {
                printf("%s %5u%s ", tile_color(gs->board[r][c]), gs->board[r][c], rst);
            } else {
                printf("       ");
            }
        }
        printf("|");
    }
    // Bottom border
    ui_term_cursor_position(start_row + GRID_SIZE * 2, start_col);
    for (int c = 0; c < GRID_SIZE; c++) {
        printf("+-------");
    }
    printf("+");
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void game2048_handler(struct command_result* res) {
    if (bp_cmd_help_check(&game2048_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;
    if (term_rows < 14 || term_cols < 40) {
        printf("Terminal too small (need at least 40x14)\r\n");
        return;
    }

    game_screen_enter(0);

    game_rng_seed();

    g2048_state_t gs = {0};

    bool quit = false;

    while (!quit) {
        init_board(&gs);

        int board_h = GRID_SIZE * 2 + 1;
        int board_w = GRID_SIZE * (TILE_W + 1) + 1;
        int start_row = (term_rows - board_h) / 2;
        int start_col = (term_cols - board_w) / 2;
        if (start_row < 1) start_row = 1;
        if (start_col < 1) start_col = 1;

        bool game_over = false;

        printf("\x1b[2J");
        while (!game_over && !quit) {
            tx_fifo_wait_drain();

            // Title (overwrite in place)
            ui_term_cursor_position(start_row - 2, start_col);
            printf("\x1b[1m2048\x1b[0m  Score: %d", gs.score);
            if (gs.won) printf("  \x1b[33m*** YOU WIN! ***\x1b[0m");
            printf("\x1b[K");

            draw_board(&gs, start_row, start_col);

            // Controls hint
            ui_term_cursor_position(start_row + board_h + 1, start_col);
            printf("WASD/arrows=slide  r=restart  q=quit");

            // Wait for input (turn-based, no timer needed)
            bool got_input = false;
            while (!got_input && !quit) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    bool moved = false;

                    if (c == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                switch (s2) {
                                    case 'A': moved = move_up(&gs);    got_input = true; break;
                                    case 'B': moved = move_down(&gs);  got_input = true; break;
                                    case 'C': moved = move_right(&gs); got_input = true; break;
                                    case 'D': moved = move_left(&gs);  got_input = true; break;
                                }
                            }
                        }
                    } else {
                        switch (c) {
                            case 'w': case 'W': moved = move_up(&gs);    got_input = true; break;
                            case 's': case 'S': moved = move_down(&gs);  got_input = true; break;
                            case 'd': case 'D': moved = move_right(&gs); got_input = true; break;
                            case 'a': case 'A': moved = move_left(&gs);  got_input = true; break;
                            case 'q': case 'Q': quit = true; break;
                            case 'r': case 'R': game_over = true; got_input = true; break;
                        }
                    }

                    if (moved) {
                        add_random_tile(&gs);
                        if (!can_move(&gs)) {
                            // Show final state then game over
                            tx_fifo_wait_drain();
                            printf("\x1b[2J");
                            ui_term_cursor_position(start_row - 2, start_col);
                            printf("\x1b[1m2048\x1b[0m  Score: %d", gs.score);
                            draw_board(&gs, start_row, start_col);
                            ui_term_cursor_position(start_row + board_h + 1, start_col);
                            printf("\x1b[1;31mGAME OVER!\x1b[0m  Score: %d  r=retry q=quit", gs.score);

                            bool decided = false;
                            while (!decided) {
                                char ch;
                                if (rx_fifo_try_get(&ch)) {
                                    if (ch == 'q' || ch == 'Q') { quit = true; decided = true; }
                                    if (ch == 'r' || ch == 'R') { game_over = true; decided = true; }
                                }
                                busy_wait_ms(10);
                            }
                        }
                    }
                }
                busy_wait_ms(5);
            }
        }
    }

    game_screen_exit();
}
