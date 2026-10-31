// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file bricks.c
 * @brief Bricks — paddle and ball brick-breaking game for Bus Pirate.
 *
 * Controls: A/D or arrows = move paddle, q = quit.
 * Ball bounces off walls, paddle, and bricks. Clear all bricks to win!
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
#include "bricks.h"

static const char* const usage[] = {
    "bricks",
    "Launch Bricks:%s bricks",
    "",
    "Break all the bricks with the bouncing ball!",
    "AD/arrows=move paddle  q=quit",
};

const bp_command_def_t bricks_def = {
    .name = "bricks",
    .description = T_HELP_BRICKS,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Game constants
// ---------------------------------------------------------------------------
#define BRK_MAX_W  40  // max field width in cells (each cell = 2 chars)
#define BRK_MAX_H  24  // max field height in rows
#define BRICK_ROWS  4
#define PADDLE_W    5

// Field state — all mutable game state collected into one struct
typedef struct {
    uint8_t bricks[BRICK_ROWS][BRK_MAX_W]; // 0=empty, 1-4=brick color
    int field_w, field_h;
    int paddle_x;
    int ball_r, ball_c;
    int ball_dr, ball_dc;
    int brk_score, lives, bricks_left;
    int tick_ms;
} bricks_state_t;

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void draw_field(bricks_state_t* gs, int sr, int sc) {
    const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";
    static const char* brick_colors[] = {
        "", "\x1b[31m", "\x1b[33m", "\x1b[32m", "\x1b[36m"
    };

    // Top wall
    ui_term_cursor_position(sr, sc);
    printf("+");
    for (int c = 0; c < gs->field_w; c++) printf("--");
    printf("+");

    for (int r = 0; r < gs->field_h; r++) {
        ui_term_cursor_position(sr + 1 + r, sc);
        printf("|");
        for (int c = 0; c < gs->field_w; c++) {
            bool is_ball = (r == gs->ball_r && c == gs->ball_c);
            bool is_paddle = (r == gs->field_h - 1 && c >= gs->paddle_x && c < gs->paddle_x + PADDLE_W);
            bool is_brick = (r < BRICK_ROWS && gs->bricks[r][c]);

            if (is_ball) {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;37mO %s", rst);
                else
                    printf("O ");
            } else if (is_brick) {
                if (system_config.terminal_ansi_color)
                    printf("%s\xe2\x96\x88\xe2\x96\x88%s", brick_colors[gs->bricks[r][c]], rst);
                else
                    printf("##");
            } else if (is_paddle) {
                if (system_config.terminal_ansi_color)
                    printf("\x1b[7m  %s", rst);
                else
                    printf("==");
            } else {
                printf("  ");
            }
        }
        printf("|");
    }

    // Bottom wall
    ui_term_cursor_position(sr + 1 + gs->field_h, sc);
    printf("+");
    for (int c = 0; c < gs->field_w; c++) printf("--");
    printf("+");

    // Status
    ui_term_cursor_position(sr + gs->field_h + 3, sc);
    printf("Score: %d  Lives: %d  Bricks: %d  %dms\x1b[K", gs->brk_score, gs->lives, gs->bricks_left, gs->tick_ms);
    ui_term_cursor_position(sr + gs->field_h + 4, sc);
    printf("AD/arrows=move  +/-=speed  q=quit");
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
static void init_game(bricks_state_t* gs) {
    memset(gs->bricks, 0, sizeof(gs->bricks));
    gs->bricks_left = 0;
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 1; c < gs->field_w - 1; c++) {
            gs->bricks[r][c] = (uint8_t)(r + 1);
            gs->bricks_left++;
        }
    }
    gs->paddle_x = (gs->field_w - PADDLE_W) / 2;
    gs->ball_r = gs->field_h - 3;
    gs->ball_c = gs->field_w / 2;
    gs->ball_dr = -1;
    gs->ball_dc = 1;
    gs->brk_score = 0;
    gs->lives = 3;
}

// ---------------------------------------------------------------------------
// Ball physics
// ---------------------------------------------------------------------------
static void tick_ball(bricks_state_t* gs) {
    int nr = gs->ball_r + gs->ball_dr;
    int nc = gs->ball_c + gs->ball_dc;

    // Wall reflections
    if (nr < 0) { nr = 0; gs->ball_dr = 1; }
    if (nc < 0) { nc = 0; gs->ball_dc = 1; }
    if (nc >= gs->field_w) { nc = gs->field_w - 1; gs->ball_dc = -1; }

    // Brick collision – check axis-aligned neighbours first so
    // the ball cannot tunnel diagonally through brick corners.
    bool bv = (nr >= 0 && nr < BRICK_ROWS && gs->bricks[nr][gs->ball_c]);
    bool bh = (gs->ball_r >= 0 && gs->ball_r < BRICK_ROWS && gs->bricks[gs->ball_r][nc]);
    bool bd = false;

    if (bv) {
        gs->bricks[nr][gs->ball_c] = 0;
        gs->bricks_left--;
        gs->brk_score += 10;
        gs->ball_dr = -gs->ball_dr;
    }
    if (bh) {
        gs->bricks[gs->ball_r][nc] = 0;
        gs->bricks_left--;
        gs->brk_score += 10;
        gs->ball_dc = -gs->ball_dc;
    }
    if (!bv && !bh && nr >= 0 && nr < BRICK_ROWS && gs->bricks[nr][nc]) {
        // Pure diagonal hit – only when no axis-aligned brick blocks path
        bd = true;
        gs->bricks[nr][nc] = 0;
        gs->bricks_left--;
        gs->brk_score += 10;
        gs->ball_dr = -gs->ball_dr;
        gs->ball_dc = -gs->ball_dc;
    }

    if (bv || bh || bd) {
        // Recompute destination after reflection
        nr = gs->ball_r + gs->ball_dr;
        nc = gs->ball_c + gs->ball_dc;
        if (nr < 0) nr = 0;
        if (nc < 0) nc = 0;
        if (nc >= gs->field_w) nc = gs->field_w - 1;
    }

    // Paddle collision
    if (nr == gs->field_h - 1 && nc >= gs->paddle_x && nc < gs->paddle_x + PADDLE_W) {
        gs->ball_dr = -1;
        nr = gs->field_h - 2;
        int hit = nc - gs->paddle_x;
        if (hit <= 1) gs->ball_dc = -1;
        else if (hit >= PADDLE_W - 2) gs->ball_dc = 1;
    }

    // Bottom — lose a life
    if (nr >= gs->field_h) {
        gs->lives--;
        gs->ball_r = gs->field_h - 3;
        gs->ball_c = gs->paddle_x + PADDLE_W / 2;
        gs->ball_dr = -1;
        gs->ball_dc = (gs->ball_dc > 0) ? 1 : -1;
        return;
    }

    gs->ball_r = nr;
    gs->ball_c = nc;
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void bricks_handler(struct command_result* res) {
    if (bp_cmd_help_check(&bricks_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;
    if (term_rows < 16 || term_cols < 30) {
        printf("Terminal too small (need at least 30x16)\r\n");
        return;
    }

    bricks_state_t gs = {0};

    gs.field_h = term_rows - 6;
    gs.field_w = (term_cols - 4) / 2;
    if (gs.field_h > BRK_MAX_H) gs.field_h = BRK_MAX_H;
    if (gs.field_w > BRK_MAX_W) gs.field_w = BRK_MAX_W;
    if (gs.field_h < 10) gs.field_h = 10;
    if (gs.field_w < 12) gs.field_w = 12;

    game_screen_enter(0);

    int fw_chars = gs.field_w * 2 + 2;
    int sr = (term_rows - gs.field_h - 5) / 2;
    int sc = (term_cols - fw_chars) / 2;
    if (sr < 1) sr = 1;
    if (sc < 1) sc = 1;

    bool quit = false;

    while (!quit) {
        init_game(&gs);
        bool playing = true;
        gs.tick_ms = 80;

        // Draw initial frame
        tx_fifo_wait_drain();
        draw_field(&gs, sr, sc);

        while (playing && !quit) {
            absolute_time_t deadline = make_timeout_time_ms(gs.tick_ms);
            int paddle_moved = 0;
            while (!time_reached(deadline)) {
                char c;
                while (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; break; }
                    if (c == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                if (s2 == 'D') c = 'a';
                                else if (s2 == 'C') c = 'd';
                                else continue;
                            } else continue;
                        } else continue;
                    }
                    if (c == 'a' || c == 'A') {
                        gs.paddle_x -= 2;
                        if (gs.paddle_x < 0) gs.paddle_x = 0;
                        paddle_moved = 1;
                    }
                    if (c == 'd' || c == 'D') {
                        gs.paddle_x += 2;
                        if (gs.paddle_x + PADDLE_W > gs.field_w) gs.paddle_x = gs.field_w - PADDLE_W;
                        paddle_moved = 1;
                    }
                    if (c == '+' || c == '=') {
                        if (gs.tick_ms > 20) gs.tick_ms -= 10;
                    }
                    if (c == '-' || c == '_') {
                        if (gs.tick_ms < 200) gs.tick_ms += 10;
                    }
                }
                if (quit) break;
                // Immediately redraw paddle row on move
                if (paddle_moved) {
                    paddle_moved = 0;
                        int pr = gs.field_h - 1;
                        ui_term_cursor_position(sr + 1 + pr, sc + 1);
                        const char* rst = system_config.terminal_ansi_color ? "\x1b[0m" : "";
                        for (int cc = 0; cc < gs.field_w; cc++) {
                            if (pr == gs.ball_r && cc == gs.ball_c) {
                                if (system_config.terminal_ansi_color)
                                    printf("\x1b[1;37mO %s", rst);
                                else
                                    printf("O ");
                            } else if (cc >= gs.paddle_x && cc < gs.paddle_x + PADDLE_W) {
                                if (system_config.terminal_ansi_color)
                                    printf("\x1b[7m  %s", rst);
                                else
                                    printf("==");
                            } else {
                                printf("  ");
                            }
                    }
                }
                busy_wait_ms(1);
            }
            if (quit) break;

            tick_ball(&gs);

            // Redraw field in-place (no screen clear = no flicker)
            tx_fifo_wait_drain();
            draw_field(&gs, sr, sc);

            if (gs.lives <= 0) {
                tx_fifo_wait_drain();
                int mid = sr + gs.field_h / 2;
                ui_term_cursor_position(mid, sc + 2);
                printf("\x1b[1;7m GAME OVER! Score: %d \x1b[0m", gs.brk_score);
                ui_term_cursor_position(mid + 1, sc + 2);
                printf("\x1b[7m r=retry q=quit \x1b[0m");
                playing = false;
            }
            if (gs.bricks_left <= 0) {
                tx_fifo_wait_drain();
                int mid = sr + gs.field_h / 2;
                ui_term_cursor_position(mid, sc + 2);
                printf("\x1b[1;32;7m YOU WIN! Score: %d \x1b[0m", gs.brk_score);
                ui_term_cursor_position(mid + 1, sc + 2);
                printf("\x1b[7m r=retry q=quit \x1b[0m");
                playing = false;
            }
        }

        if (!quit) {
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
