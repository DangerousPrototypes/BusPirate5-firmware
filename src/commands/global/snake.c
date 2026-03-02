// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file snake.c
 * @brief Snake — classic terminal snake game for Bus Pirate.
 *
 * Controls: WASD or arrow keys to steer, q to quit.
 * The snake wraps around the edges (toroidal).
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
#include "snake.h"

// ---------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------
#define SNAKE_MAX_LEN 256
#define SNAKE_MAX_ROWS 48
#define SNAKE_MAX_COLS 78

// Directions
enum { DIR_UP = 0, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

typedef struct {
    uint8_t r, c;
} pos_t;

// ---------------------------------------------------------------------------
// Command definition
// ---------------------------------------------------------------------------
static const char* const usage[] = {
    "snake",
    "Launch Snake game:%s snake",
    "",
    "Classic snake game. Eat food to grow, don't hit yourself!",
    "WASD/arrows=move  q=quit",
};

const bp_command_def_t snake_def = {
    .name = "snake",
    .description = T_HELP_SNAKE,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------
static void draw_cell(int r, int c, const char* ch) {
    ui_term_cursor_position(r + 1, c * 2 + 1);
    printf("%s", ch);
}

static void draw_border(int rows, int cols) {
    // Top border
    ui_term_cursor_position(1, 1);
    for (int c = 0; c < cols; c++) printf("\xe2\x94\x80\xe2\x94\x80"); // ──
    // Bottom border
    ui_term_cursor_position(rows + 2, 1);
    for (int c = 0; c < cols; c++) printf("\xe2\x94\x80\xe2\x94\x80"); // ──
}

static void draw_status(int row, int term_cols, int score, int hi_score) {
    ui_term_cursor_position(row, 1);
    printf("\x1b[7m");
    printf(" Snake | Score: %04d | Best: %04d | WASD/arrows=move q=quit ", score, hi_score);
    ui_term_erase_line();
    printf("\x1b[0m");
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef struct {
    pos_t body[SNAKE_MAX_LEN];
    int snake_len;
    int direction;
    pos_t food;
    int rows;
    int cols;
} snake_state_t;

static bool occupied(snake_state_t* gs, int r, int c) {
    for (int i = 0; i < gs->snake_len; i++) {
        if (gs->body[i].r == r && gs->body[i].c == c) return true;
    }
    return false;
}

static void place_food(snake_state_t* gs) {
    int attempts = 0;
    do {
        gs->food.r = game_rng_next() % gs->rows;
        gs->food.c = game_rng_next() % gs->cols;
        attempts++;
    } while (occupied(gs, gs->food.r, gs->food.c) && attempts < 1000);
}

static bool try_read_direction(snake_state_t* gs) {
    char c;
    if (!rx_fifo_try_get(&c)) return false;

    if (c == 'q' || c == 'Q') return true; // signals quit via special handling

    // Arrow key ESC sequence: ESC [ A/B/C/D
    if (c == 0x1b) {
        char seq1, seq2;
        busy_wait_ms(2);
        if (!rx_fifo_try_get(&seq1)) return false;
        if (seq1 != '[') return false;
        busy_wait_ms(2);
        if (!rx_fifo_try_get(&seq2)) return false;
        switch (seq2) {
            case 'A': c = 'w'; break;
            case 'B': c = 's'; break;
            case 'C': c = 'd'; break;
            case 'D': c = 'a'; break;
            default: return false;
        }
    }

    switch (c) {
        case 'w': case 'W': if (gs->direction != DIR_DOWN)  gs->direction = DIR_UP;    break;
        case 's': case 'S': if (gs->direction != DIR_UP)    gs->direction = DIR_DOWN;  break;
        case 'a': case 'A': if (gs->direction != DIR_RIGHT) gs->direction = DIR_LEFT;  break;
        case 'd': case 'D': if (gs->direction != DIR_LEFT)  gs->direction = DIR_RIGHT; break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void snake_handler(struct command_result* res) {
    if (bp_cmd_help_check(&snake_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;
    if (term_rows < 8 || term_cols < 20) {
        printf("Terminal too small (need at least 20x8)\r\n");
        return;
    }

    // Game area: leave 1 row for status at bottom, game area rows 1..term_rows-1
    snake_state_t gs = {0};

    gs.rows = term_rows - 1;
    gs.cols = term_cols / 2;
    if (gs.rows > SNAKE_MAX_ROWS) gs.rows = SNAKE_MAX_ROWS;
    if (gs.cols > SNAKE_MAX_COLS) gs.cols = SNAKE_MAX_COLS;

    game_screen_enter(gs.rows);

    game_rng_seed();

    int hi_score = 0;
    bool quit = false;

    while (!quit) {
        // Init snake in center
        gs.snake_len = 3;
        gs.direction = DIR_RIGHT;
        int start_r = gs.rows / 2;
        int start_c = gs.cols / 2;
        for (int i = 0; i < gs.snake_len; i++) {
            gs.body[i].r = start_r;
            gs.body[i].c = start_c - i;
        }
        place_food(&gs);

        // Draw initial state
        printf("\x1b[2J");
        for (int i = 0; i < gs.snake_len; i++) {
            draw_cell(gs.body[i].r, gs.body[i].c,
                      system_config.terminal_ansi_color ? "\x1b[32m\xe2\x96\x88\xe2\x96\x88\x1b[0m" : "##");
        }
        draw_cell(gs.food.r, gs.food.c,
                  system_config.terminal_ansi_color ? "\x1b[31m\xe2\x97\x8f\xe2\x97\x8f\x1b[0m" : "@@");

        int score = 0;
        bool alive = true;
        int speed_ms = 120;

        while (alive && !quit) {
            tx_fifo_wait_drain();
            draw_status(term_rows, term_cols, score, hi_score);

            // Wait for tick, polling input
            absolute_time_t deadline = make_timeout_time_ms(speed_ms);
            while (!time_reached(deadline)) {
                char peek;
                if (rx_fifo_try_get(&peek)) {
                    if (peek == 'q' || peek == 'Q') { quit = true; break; }
                    if (peek == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                switch (s2) {
                                    case 'A': if (gs.direction != DIR_DOWN)  gs.direction = DIR_UP;    break;
                                    case 'B': if (gs.direction != DIR_UP)    gs.direction = DIR_DOWN;  break;
                                    case 'C': if (gs.direction != DIR_LEFT)  gs.direction = DIR_RIGHT; break;
                                    case 'D': if (gs.direction != DIR_RIGHT) gs.direction = DIR_LEFT;  break;
                                }
                            }
                        }
                    } else {
                        switch (peek) {
                            case 'w': case 'W': if (gs.direction != DIR_DOWN)  gs.direction = DIR_UP;    break;
                            case 's': case 'S': if (gs.direction != DIR_UP)    gs.direction = DIR_DOWN;  break;
                            case 'a': case 'A': if (gs.direction != DIR_RIGHT) gs.direction = DIR_LEFT;  break;
                            case 'd': case 'D': if (gs.direction != DIR_LEFT)  gs.direction = DIR_RIGHT; break;
                        }
                    }
                }
                busy_wait_ms(1);
            }
            if (quit) break;

            // Move head
            pos_t new_head = gs.body[0];
            switch (gs.direction) {
                case DIR_UP:    new_head.r--; break;
                case DIR_DOWN:  new_head.r++; break;
                case DIR_LEFT:  new_head.c--; break;
                case DIR_RIGHT: new_head.c++; break;
            }

            // Wrap
            if (new_head.r < 0) new_head.r = gs.rows - 1;
            if (new_head.r >= gs.rows) new_head.r = 0;
            if (new_head.c < 0) new_head.c = gs.cols - 1;
            if (new_head.c >= gs.cols) new_head.c = 0;

            // Self-collision?
            for (int i = 0; i < gs.snake_len; i++) {
                if (gs.body[i].r == new_head.r && gs.body[i].c == new_head.c) {
                    alive = false;
                    break;
                }
            }
            if (!alive) break;

            bool ate = (new_head.r == gs.food.r && new_head.c == gs.food.c);

            if (!ate) {
                // Erase tail
                draw_cell(gs.body[gs.snake_len - 1].r, gs.body[gs.snake_len - 1].c, "  ");
            }

            // Shift body
            if (ate && gs.snake_len < SNAKE_MAX_LEN) {
                gs.snake_len++;
            }
            for (int i = gs.snake_len - 1; i > 0; i--) {
                gs.body[i] = gs.body[i - 1];
            }
            gs.body[0] = new_head;

            // Draw head
            draw_cell(new_head.r, new_head.c,
                      system_config.terminal_ansi_color ? "\x1b[32m\xe2\x96\x88\xe2\x96\x88\x1b[0m" : "##");

            if (ate) {
                score += 10;
                if (score > hi_score) hi_score = score;
                // Speed up slightly
                if (speed_ms > 40) speed_ms -= 2;
                place_food(&gs);
                draw_cell(gs.food.r, gs.food.c,
                          system_config.terminal_ansi_color ? "\x1b[31m\xe2\x97\x8f\xe2\x97\x8f\x1b[0m" : "@@");
            }
        }

        if (!quit) {
            // Game over message
            int mid_r = gs.rows / 2;
            int mid_c = (gs.cols * 2 - 30) / 2;
            if (mid_c < 1) mid_c = 1;
            ui_term_cursor_position(mid_r, mid_c);
            printf("\x1b[7m GAME OVER! Score: %d \x1b[0m", score);
            ui_term_cursor_position(mid_r + 1, mid_c);
            printf("\x1b[7m r=retry  q=quit \x1b[0m");

            // Wait for r or q
            bool decided = false;
            while (!decided) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; decided = true; }
                    if (c == 'r' || c == 'R') { decided = true; }
                }
                busy_wait_ms(10);
            }
        }
    }

    game_screen_exit();
}
