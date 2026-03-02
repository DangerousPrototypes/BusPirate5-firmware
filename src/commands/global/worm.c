// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file worm.c
 * @brief Worm — BSD worm clone for Bus Pirate.
 *
 * A worm moves around the screen eating numbered food (1-9).
 * Each food adds that many segments to the worm's length.
 * Avoid walls and yourself. Speed increases as you grow.
 * Controls: WASD or arrow keys to steer, q to quit.
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
#include "worm.h"

#define WORM_MAX_LEN 256
#define WORM_MIN_ROWS 12
#define WORM_MIN_COLS 30

static const char* const usage[] = {
    "worm",
    "Launch Worm:%s worm",
    "",
    "Eat numbered food to grow! Each number adds that many segments.",
    "arrows/wasd=steer  q=quit",
};

const bp_command_def_t worm_def = {
    .name = "worm",
    .description = T_HELP_WORM,
    .usage = usage,
    .usage_count = count_of(usage),
};

// Directions
enum { DIR_UP = 0, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

// Worm state
typedef struct {
    struct {
        uint8_t r;
        uint8_t c;
    } body[WORM_MAX_LEN];
    int head_idx, tail_idx, worm_len;
    int grow_pending; // segments to add
    int dir;
    int food_r, food_c, food_val;
    int score;
    int area_rows, area_cols; // play area (inside border)
    int off_r, off_c;        // offset of play area on screen
} worm_state_t;

static void place_food(worm_state_t* gs) {
    for (int attempts = 0; attempts < 500; attempts++) {
        int r = 1 + (game_rng_next() % gs->area_rows);
        int c = 1 + (game_rng_next() % gs->area_cols);
        // Check not on worm
        bool on_worm = false;
        int idx = gs->tail_idx;
        for (int i = 0; i < gs->worm_len; i++) {
            if (gs->body[idx].r == r && gs->body[idx].c == c) { on_worm = true; break; }
            idx = (idx + 1) % WORM_MAX_LEN;
        }
        if (!on_worm) {
            gs->food_r = r;
            gs->food_c = c;
            gs->food_val = 1 + (game_rng_next() % 9); // 1-9
            return;
        }
    }
    // Fallback: place anywhere
    gs->food_r = 1;
    gs->food_c = 1;
    gs->food_val = 1;
}

static void draw_border(worm_state_t* gs) {
    bool color = system_config.terminal_ansi_color;
    const char* bdr = color ? "\x1b[1;37m" : "";
    const char* rst = color ? "\x1b[0m" : "";

    // Top
    ui_term_cursor_position(gs->off_r, gs->off_c);
    printf("%s+", bdr);
    for (int c = 0; c < gs->area_cols; c++) printf("-");
    printf("+%s", rst);

    // Sides
    for (int r = 1; r <= gs->area_rows; r++) {
        ui_term_cursor_position(gs->off_r + r, gs->off_c);
        printf("%s|%s", bdr, rst);
        ui_term_cursor_position(gs->off_r + r, gs->off_c + gs->area_cols + 1);
        printf("%s|%s", bdr, rst);
    }

    // Bottom
    ui_term_cursor_position(gs->off_r + gs->area_rows + 1, gs->off_c);
    printf("%s+", bdr);
    for (int c = 0; c < gs->area_cols; c++) printf("-");
    printf("+%s", rst);
}

static void draw_cell(worm_state_t* gs, int r, int c, char ch, const char* color) {
    bool use_color = system_config.terminal_ansi_color;
    ui_term_cursor_position(gs->off_r + r, gs->off_c + c);
    if (use_color && color) printf("%s%c\x1b[0m", color, ch);
    else printf("%c", ch);
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void worm_handler(struct command_result* res) {
    if (bp_cmd_help_check(&worm_def, res->help_flag)) return;

    worm_state_t gs = {0};

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;

    // Play area inside border
    gs.area_rows = term_rows - 4; // leave room for border + status
    gs.area_cols = term_cols - 4;
    if (gs.area_rows < WORM_MIN_ROWS) gs.area_rows = WORM_MIN_ROWS;
    if (gs.area_cols < WORM_MIN_COLS) gs.area_cols = WORM_MIN_COLS;
    if (gs.area_rows > 40) gs.area_rows = 40;
    if (gs.area_cols > 76) gs.area_cols = 76;

    gs.off_r = 1;
    gs.off_c = 1;

    game_screen_enter(0);

    game_rng_seed();

    bool quit = false;

    while (!quit) {
        // Init worm in center
        gs.head_idx = 0;
        gs.tail_idx = 0;
        gs.worm_len = 3;
        gs.grow_pending = 0;
        gs.score = 0;
        gs.dir = DIR_RIGHT;

        int mid_r = 1 + gs.area_rows / 2;
        int mid_c = 1 + gs.area_cols / 2;
        for (int i = 0; i < gs.worm_len; i++) {
            int idx = i;
            gs.body[idx].r = mid_r;
            gs.body[idx].c = mid_c - (gs.worm_len - 1) + i;
        }
        gs.head_idx = gs.worm_len - 1;
        gs.tail_idx = 0;

        place_food(&gs);

        printf("\x1b[2J");
        draw_border(&gs);

        // Draw initial worm
        int idx = gs.tail_idx;
        for (int i = 0; i < gs.worm_len; i++) {
            if (i == gs.head_idx)
                draw_cell(&gs, gs.body[idx].r, gs.body[idx].c, '@', "\x1b[1;32m");
            else
                draw_cell(&gs, gs.body[idx].r, gs.body[idx].c, 'o', "\x1b[32m");
            idx = (idx + 1) % WORM_MAX_LEN;
        }

        // Draw food
        char fc = '0' + gs.food_val;
        draw_cell(&gs, gs.food_r, gs.food_c, fc, "\x1b[1;33m");

        // Status
        ui_term_cursor_position(gs.off_r + gs.area_rows + 2, gs.off_c);
        printf("Score: %4d  Len: %3d  arrows/wasd=steer  q=quit", gs.score, gs.worm_len);

        bool alive = true;
        uint32_t tick_ms = 150;

        while (alive && !quit) {
            uint32_t start = time_us_32();

            // Process input
            char c;
            while (rx_fifo_try_get(&c)) {
                if (c == 'q' || c == 'Q') { quit = true; break; }
                if (c == 'w' || c == 'W') { if (gs.dir != DIR_DOWN) gs.dir = DIR_UP; }
                else if (c == 's' || c == 'S') { if (gs.dir != DIR_UP) gs.dir = DIR_DOWN; }
                else if (c == 'a' || c == 'A') { if (gs.dir != DIR_RIGHT) gs.dir = DIR_LEFT; }
                else if (c == 'd' || c == 'D') { if (gs.dir != DIR_LEFT) gs.dir = DIR_RIGHT; }
                else if (c == 0x1b) {
                    char s1, s2;
                    busy_wait_ms(2);
                    if (rx_fifo_try_get(&s1) && s1 == '[') {
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s2)) {
                            if (s2 == 'A') { if (gs.dir != DIR_DOWN) gs.dir = DIR_UP; }
                            else if (s2 == 'B') { if (gs.dir != DIR_UP) gs.dir = DIR_DOWN; }
                            else if (s2 == 'D') { if (gs.dir != DIR_RIGHT) gs.dir = DIR_LEFT; }
                            else if (s2 == 'C') { if (gs.dir != DIR_LEFT) gs.dir = DIR_RIGHT; }
                        }
                    }
                }
            }
            if (quit) break;

            // Move head
            int new_r = gs.body[gs.head_idx].r;
            int new_c = gs.body[gs.head_idx].c;
            switch (gs.dir) {
                case DIR_UP:    new_r--; break;
                case DIR_DOWN:  new_r++; break;
                case DIR_LEFT:  new_c--; break;
                case DIR_RIGHT: new_c++; break;
            }

            // Wall collision
            if (new_r < 1 || new_r > gs.area_rows || new_c < 1 || new_c > gs.area_cols) {
                alive = false;
                break;
            }

            // Self collision
            idx = gs.tail_idx;
            for (int i = 0; i < gs.worm_len; i++) {
                if (gs.body[idx].r == new_r && gs.body[idx].c == new_c) {
                    alive = false;
                    break;
                }
                idx = (idx + 1) % WORM_MAX_LEN;
            }
            if (!alive) break;

            // Erase old head marker (draw body char)
            draw_cell(&gs, gs.body[gs.head_idx].r, gs.body[gs.head_idx].c, 'o', "\x1b[32m");

            // Add new head
            gs.head_idx = (gs.head_idx + 1) % WORM_MAX_LEN;
            gs.body[gs.head_idx].r = new_r;
            gs.body[gs.head_idx].c = new_c;
            gs.worm_len++;

            // Draw new head
            draw_cell(&gs, new_r, new_c, '@', "\x1b[1;32m");

            // Check food
            if (new_r == gs.food_r && new_c == gs.food_c) {
                gs.grow_pending += gs.food_val - 1; // we already grew by 1
                gs.score += gs.food_val * 10;
                place_food(&gs);
                char fch = '0' + gs.food_val;
                draw_cell(&gs, gs.food_r, gs.food_c, fch, "\x1b[1;33m");

                // Speed up
                if (tick_ms > 60) tick_ms -= 3;
            } else {
                // Remove tail
                if (gs.grow_pending > 0) {
                    gs.grow_pending--;
                } else {
                    draw_cell(&gs, gs.body[gs.tail_idx].r, gs.body[gs.tail_idx].c, ' ', NULL);
                    gs.tail_idx = (gs.tail_idx + 1) % WORM_MAX_LEN;
                    gs.worm_len--;
                }
            }

            // Prevent overflow
            if (gs.worm_len >= WORM_MAX_LEN - 1) {
                draw_cell(&gs, gs.body[gs.tail_idx].r, gs.body[gs.tail_idx].c, ' ', NULL);
                gs.tail_idx = (gs.tail_idx + 1) % WORM_MAX_LEN;
                gs.worm_len--;
                gs.grow_pending = 0;
            }

            // Update status
            tx_fifo_wait_drain();
            ui_term_cursor_position(gs.off_r + gs.area_rows + 2, gs.off_c);
            printf("Score: %4d  Len: %3d  arrows/wasd=steer  q=quit", gs.score, gs.worm_len);

            // Tick timing
            uint32_t elapsed = (time_us_32() - start) / 1000;
            if (elapsed < tick_ms) busy_wait_ms(tick_ms - elapsed);
        }

        if (!quit) {
            // Game over
            tx_fifo_wait_drain();
            int msg_r = gs.off_r + gs.area_rows / 2;
            int msg_c = gs.off_c + gs.area_cols / 2 - 10;
            if (msg_c < gs.off_c + 1) msg_c = gs.off_c + 1;
            ui_term_cursor_position(msg_r, msg_c);
            printf("\x1b[1;31m GAME OVER! Score: %d \x1b[0m", gs.score);
            ui_term_cursor_position(msg_r + 1, msg_c);
            printf("  r=restart  q=quit");

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
