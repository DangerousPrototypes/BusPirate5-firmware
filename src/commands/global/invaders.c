// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file invaders.c
 * @brief Protocol Invaders — Space Invaders for Bus Pirate.
 *
 * Shoot down malformed packets descending toward your logic probe.
 * Enemies: 0xDEAD, 0xBAD, NAK, FAULT, 0xFF, CRC!, NOPE, OOPS
 * Barriers are decoupling caps that degrade when hit.
 * Controls: LEFT/RIGHT = move, SPACE = fire, q = quit.
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
#include "invaders.h"

static const char* const usage[] = {
    "invaders",
    "Launch Protocol Invaders:%s invaders",
    "",
    "Shoot down malformed packets raining from the bus!",
    "LEFT/RIGHT=move  SPACE=fire  q=quit",
};

const bp_command_def_t invaders_def = {
    .name = "invaders",
    .description = T_HELP_INVADERS,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define INV_COLS        8       // enemies per row
#define INV_ROWS        4       // rows of enemies
#define INV_TOTAL       (INV_COLS * INV_ROWS)
#define ENEMY_SPACING_X 8       // horizontal spacing between enemy centers
#define ENEMY_SPACING_Y 2       // vertical spacing between enemy rows
#define BARRIER_COUNT   4       // number of decoupling cap barriers
#define BARRIER_HP      4       // hits before barrier destroyed
#define MAX_BULLETS     4       // player bullets on screen at once
#define MAX_BOMBS       8       // enemy bombs on screen at once
#define PLAYER_CHAR     '^'
#define BULLET_CHAR     '|'
#define BOMB_CHAR       '*'
#define TICK_MS_INIT    80      // initial tick speed
#define TICK_MS_MIN     30      // fastest tick speed
#define ENEMY_MOVE_RATE 3       // enemy fleet moves every N ticks
#define ENEMY_DROP_ROWS 1       // rows to drop when hitting edge
#define BOMB_RATE       40      // 1-in-N chance per tick per living enemy column

// Enemy labels (padded to 5 chars for uniform grid)
static const char* const enemy_labels[] = {
    "xDEAD", " xBAD", " NAK ", "FAULT",
    " xFF ", " CRC!", " NOPE", " OOPS",
    "xBEEF", " ERR ", " xF0 ", "GLITC",
};
#define ENEMY_LABEL_COUNT (sizeof(enemy_labels) / sizeof(enemy_labels[0]))
#define ENEMY_WIDTH 5

// Barrier art (degrades over 4 stages)
static const char* const barrier_art[] = {
    "[===]",    // full
    "[== ]",    // stage 1
    "[ = ]",    // stage 2
    "[   ]",    // stage 3 (barely there)
};

// ANSI colors
static const char* const grn = "\x1b[32m";
static const char* const red = "\x1b[31m";
static const char* const ylw = "\x1b[33m";
static const char* const cyn = "\x1b[36m";
static const char* const wht = "\x1b[1;37m";
static const char* const mag = "\x1b[35m";
static const char* const rst = "\x1b[0m";
static const char* const dim = "\x1b[2m";

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------

typedef struct {
    bool alive;
    int label_idx;               // index into enemy_labels
} enemy_t;

typedef struct {
    bool active;
    int row, col;                // 1-based
} bullet_t;

typedef struct {
    int col;                     // center column (1-based)
    int row;                     // row (1-based)
    int hp;                      // 0 = destroyed
} barrier_t;

typedef struct {
    int field_w;             // playing field width (terminal cols)
    int field_h;             // playing field height (terminal rows - 1 for status)
    int player_x;            // player column (1-based)
    int player_row;          // player row (1-based, near bottom)
    int lives;
    int score;
    int hi_score;
    int level;
    enemy_t enemies[INV_ROWS][INV_COLS];
    int fleet_top_row;        // top-left row of the fleet grid (1-based)
    int fleet_left_col;       // top-left col of the fleet grid (1-based)
    int fleet_dir;            // +1 = right, -1 = left
    int fleet_move_timer;     // counts ticks until next fleet move
    int enemies_alive;
    bullet_t bullets[MAX_BULLETS];
    bullet_t bombs[MAX_BOMBS];
    barrier_t barriers[BARRIER_COUNT];
} invaders_state_t;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void init_enemies(invaders_state_t* gs) {
    gs->enemies_alive = 0;
    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            gs->enemies[r][c].alive = true;
            gs->enemies[r][c].label_idx = (int)(game_rng_next() % ENEMY_LABEL_COUNT);
            gs->enemies_alive++;
        }
    }
    // Center the fleet horizontally
    int fleet_grid_w = INV_COLS * ENEMY_SPACING_X;
    gs->fleet_left_col = (gs->field_w - fleet_grid_w) / 2 + 1;
    gs->fleet_top_row = 2;
    gs->fleet_dir = 1;
    gs->fleet_move_timer = ENEMY_MOVE_RATE;
}

static void init_barriers(invaders_state_t* gs) {
    int spacing = gs->field_w / (BARRIER_COUNT + 1);
    int bar_row = gs->player_row - 4;
    for (int i = 0; i < BARRIER_COUNT; i++) {
        gs->barriers[i].col = spacing * (i + 1);
        gs->barriers[i].row = bar_row;
        gs->barriers[i].hp = BARRIER_HP;
    }
}

static void init_bullets(invaders_state_t* gs) {
    for (int i = 0; i < MAX_BULLETS; i++) gs->bullets[i].active = false;
    for (int i = 0; i < MAX_BOMBS; i++) gs->bombs[i].active = false;
}

// Get the screen position of an enemy
static void enemy_screen_pos(invaders_state_t* gs, int r, int c, int* row, int* col) {
    *row = gs->fleet_top_row + r * ENEMY_SPACING_Y;
    *col = gs->fleet_left_col + c * ENEMY_SPACING_X;
}

// Find leftmost and rightmost living enemy columns
static int fleet_left_extent(invaders_state_t* gs) {
    for (int c = 0; c < INV_COLS; c++)
        for (int r = 0; r < INV_ROWS; r++)
            if (gs->enemies[r][c].alive) {
                int er, ec;
                enemy_screen_pos(gs, r, c, &er, &ec);
                return ec;
            }
    return gs->fleet_left_col;
}

static int fleet_right_extent(invaders_state_t* gs) {
    for (int c = INV_COLS - 1; c >= 0; c--)
        for (int r = 0; r < INV_ROWS; r++)
            if (gs->enemies[r][c].alive) {
                int er, ec;
                enemy_screen_pos(gs, r, c, &er, &ec);
                return ec + ENEMY_WIDTH - 1;
            }
    return gs->fleet_left_col;
}

// Find bottom-most living enemy row
static int fleet_bottom_extent(invaders_state_t* gs) {
    for (int r = INV_ROWS - 1; r >= 0; r--)
        for (int c = 0; c < INV_COLS; c++)
            if (gs->enemies[r][c].alive) {
                int er, ec;
                enemy_screen_pos(gs, r, c, &er, &ec);
                return er;
            }
    return gs->fleet_top_row;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void draw_status(invaders_state_t* gs, int status_row) {
    ui_term_cursor_position(status_row, 1);
    printf(" %sINVADERS%s S:%s%05d%s Hi:%s%05d%s x%s%d%s Lv:%s%d%s </>=move SPC=fire q=quit",
           wht, rst, ylw, gs->score, rst, red, gs->hi_score, rst,
           grn, gs->lives, rst, cyn, gs->level, rst);
    printf("\x1b[K");
}

// Color palette indices for flicker-free line-buffer rendering
#define CLR_0   0
#define CLR_GRN 1
#define CLR_RED 2
#define CLR_YLW 3
#define CLR_CYN 4
#define CLR_WHT 5
#define CLR_MAG 6

static const char* const clr_lut[] = {
    "\x1b[0m", "\x1b[32m", "\x1b[31m", "\x1b[33m",
    "\x1b[36m", "\x1b[1;37m", "\x1b[35m",
};

#define ROW_BUF_W 120

// Emit one composited row — no flicker (overwrite, never clear first)
// NOTE: uses printf (custom _putchar) instead of libc putchar, which
//       does not route to USB output on this bare-metal firmware.
static void emit_row(int row, const char* rbuf, const uint8_t* rclr, int w) {
    bool color = system_config.terminal_ansi_color;
    ui_term_cursor_position(row, 1);
    if (color) {
        uint8_t cur = 255;
        int span_start = 0;
        for (int i = 0; i < w; i++) {
            if (rclr[i] != cur) {
                // Flush previous span
                if (i > span_start) {
                    printf("%.*s", i - span_start, &rbuf[span_start]);
                }
                cur = rclr[i];
                printf("%s", clr_lut[cur]);
                span_start = i;
            }
        }
        // Flush final span
        if (w > span_start) {
            printf("%.*s", w - span_start, &rbuf[span_start]);
        }
        printf("\x1b[0m");
    } else {
        printf("%.*s", w, rbuf);
    }
    printf("\x1b[K");
}

static void draw_frame(invaders_state_t* gs, int status_row) {
    int w = gs->field_w;
    if (w > ROW_BUF_W) w = ROW_BUF_W;
    char rbuf[ROW_BUF_W];
    uint8_t rclr[ROW_BUF_W];

    for (int row = 1; row <= gs->player_row; row++) {
        memset(rbuf, ' ', w);
        memset(rclr, CLR_0, w);

        // --- Stamp enemies ---
        for (int r = 0; r < INV_ROWS; r++) {
            for (int c = 0; c < INV_COLS; c++) {
                if (!gs->enemies[r][c].alive) continue;
                int er, ec;
                enemy_screen_pos(gs, r, c, &er, &ec);
                if (er != row) continue;
                uint8_t clr = (r == 0) ? CLR_RED : (r == 1) ? CLR_MAG : (r == 2) ? CLR_YLW : CLR_CYN;
                const char* lbl = enemy_labels[gs->enemies[r][c].label_idx];
                for (int k = 0; k < ENEMY_WIDTH; k++) {
                    int pos = ec - 1 + k;
                    if (pos >= 0 && pos < w) {
                        rbuf[pos] = lbl[k];
                        rclr[pos] = clr;
                    }
                }
            }
        }

        // --- Stamp barriers ---
        for (int i = 0; i < BARRIER_COUNT; i++) {
            if (gs->barriers[i].hp <= 0 || gs->barriers[i].row != row) continue;
            int idx = BARRIER_HP - gs->barriers[i].hp;
            if (idx < 0) idx = 0;
            if (idx > 3) idx = 3;
            const char* art = barrier_art[idx];
            int col0 = gs->barriers[i].col - 2 - 1;
            for (int k = 0; k < 5; k++) {
                int pos = col0 + k;
                if (pos >= 0 && pos < w) {
                    rbuf[pos] = art[k];
                    rclr[pos] = CLR_GRN;
                }
            }
        }

        // --- Stamp bullets ---
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!gs->bullets[i].active || gs->bullets[i].row != row) continue;
            int pos = gs->bullets[i].col - 1;
            if (pos >= 0 && pos < w) {
                rbuf[pos] = BULLET_CHAR;
                rclr[pos] = CLR_YLW;
            }
        }

        // --- Stamp bombs ---
        for (int i = 0; i < MAX_BOMBS; i++) {
            if (!gs->bombs[i].active || gs->bombs[i].row != row) continue;
            int pos = gs->bombs[i].col - 1;
            if (pos >= 0 && pos < w) {
                rbuf[pos] = BOMB_CHAR;
                rclr[pos] = CLR_RED;
            }
        }

        // --- Stamp player (last = on top) ---
        if (row == gs->player_row) {
            int pos = gs->player_x - 2;
            if (pos >= 0 && pos < w) { rbuf[pos] = '/'; rclr[pos] = CLR_GRN; }
            pos++;
            if (pos >= 0 && pos < w) { rbuf[pos] = PLAYER_CHAR; rclr[pos] = CLR_GRN; }
            pos++;
            if (pos >= 0 && pos < w) { rbuf[pos] = '\\'; rclr[pos] = CLR_GRN; }
        }

        emit_row(row, rbuf, rclr, w);
    }

    draw_status(gs, status_row);
}



// ---------------------------------------------------------------------------
// Game logic
// ---------------------------------------------------------------------------

static void fire_bullet(invaders_state_t* gs) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!gs->bullets[i].active) {
            gs->bullets[i].active = true;
            gs->bullets[i].row = gs->player_row - 1;
            gs->bullets[i].col = gs->player_x;
            return;
        }
    }
}

static void move_bullets(invaders_state_t* gs) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!gs->bullets[i].active) continue;
        gs->bullets[i].row--;
        if (gs->bullets[i].row < 1) {
            gs->bullets[i].active = false;
        }
    }
}

static void drop_bombs(invaders_state_t* gs) {
    // Each living bottom-most enemy in a column has a chance to drop
    for (int c = 0; c < INV_COLS; c++) {
        // Find bottom-most alive enemy in this column
        int br = -1;
        for (int r = INV_ROWS - 1; r >= 0; r--) {
            if (gs->enemies[r][c].alive) { br = r; break; }
        }
        if (br < 0) continue;
        if ((int)(game_rng_next() % BOMB_RATE) != 0) continue;

        // Find free bomb slot
        for (int i = 0; i < MAX_BOMBS; i++) {
            if (!gs->bombs[i].active) {
                int er, ec;
                enemy_screen_pos(gs, br, c, &er, &ec);
                gs->bombs[i].active = true;
                gs->bombs[i].row = er + 1;
                gs->bombs[i].col = ec + ENEMY_WIDTH / 2;
                break;
            }
        }
    }
}

static void move_bombs(invaders_state_t* gs) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!gs->bombs[i].active) continue;
        gs->bombs[i].row++;
        if (gs->bombs[i].row > gs->player_row) {
            gs->bombs[i].active = false;
        }
    }
}

static void move_fleet(invaders_state_t* gs) {
    gs->fleet_move_timer--;
    if (gs->fleet_move_timer > 0) return;
    gs->fleet_move_timer = ENEMY_MOVE_RATE;

    // Check bounds
    int le = fleet_left_extent(gs);
    int re = fleet_right_extent(gs);
    bool need_drop = false;

    if (gs->fleet_dir > 0 && re >= gs->field_w - 1) {
        gs->fleet_dir = -1;
        need_drop = true;
    } else if (gs->fleet_dir < 0 && le <= 2) {
        gs->fleet_dir = 1;
        need_drop = true;
    }

    if (need_drop) {
        gs->fleet_top_row += ENEMY_DROP_ROWS;
    } else {
        gs->fleet_left_col += gs->fleet_dir;
    }
}

// Check bullet-enemy collisions
static int check_bullet_hits(invaders_state_t* gs) {
    int hits = 0;
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!gs->bullets[b].active) continue;
        for (int r = 0; r < INV_ROWS; r++) {
            for (int c = 0; c < INV_COLS; c++) {
                if (!gs->enemies[r][c].alive) continue;
                int er, ec;
                enemy_screen_pos(gs, r, c, &er, &ec);
                if (gs->bullets[b].row == er &&
                    gs->bullets[b].col >= ec &&
                    gs->bullets[b].col < ec + ENEMY_WIDTH) {
                    gs->enemies[r][c].alive = false;
                    gs->bullets[b].active = false;
                    gs->enemies_alive--;
                    hits++;
                    // Points: top row=40, next=30, 20, 10
                    gs->score += (INV_ROWS - r) * 10;
                    // Erase enemy
                    ui_term_cursor_position(er, ec);
                    printf("%s*BAM*%s", ylw, rst);
                    break;
                }
            }
            if (!gs->bullets[b].active) break;
        }
    }
    return hits;
}

// Check bullet-barrier collisions
static void check_bullet_barrier(invaders_state_t* gs) {
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!gs->bullets[b].active) continue;
        for (int i = 0; i < BARRIER_COUNT; i++) {
            if (gs->barriers[i].hp <= 0) continue;
            if (gs->bullets[b].row == gs->barriers[i].row &&
                gs->bullets[b].col >= gs->barriers[i].col - 2 &&
                gs->bullets[b].col <= gs->barriers[i].col + 2) {
                gs->bullets[b].active = false;
                gs->barriers[i].hp--;
                break;
            }
        }
    }
}

// Check bomb-barrier collisions
static void check_bomb_barrier(invaders_state_t* gs) {
    for (int b = 0; b < MAX_BOMBS; b++) {
        if (!gs->bombs[b].active) continue;
        for (int i = 0; i < BARRIER_COUNT; i++) {
            if (gs->barriers[i].hp <= 0) continue;
            if (gs->bombs[b].row == gs->barriers[i].row &&
                gs->bombs[b].col >= gs->barriers[i].col - 2 &&
                gs->bombs[b].col <= gs->barriers[i].col + 2) {
                gs->bombs[b].active = false;
                gs->barriers[i].hp--;
                break;
            }
        }
    }
}

// Check bomb-player collisions
static bool check_bomb_player(invaders_state_t* gs) {
    for (int b = 0; b < MAX_BOMBS; b++) {
        if (!gs->bombs[b].active) continue;
        if (gs->bombs[b].row == gs->player_row &&
            gs->bombs[b].col >= gs->player_x - 1 &&
            gs->bombs[b].col <= gs->player_x + 1) {
            gs->bombs[b].active = false;
            return true;
        }
    }
    return false;
}

// Check if enemies have reached player row
static bool enemies_reached_bottom(invaders_state_t* gs) {
    return fleet_bottom_extent(gs) >= gs->player_row - 2;
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void invaders_handler(struct command_result* res) {
    if (bp_cmd_help_check(&invaders_def, res->help_flag)) return;

    invaders_state_t gs = {0};

    // Get terminal size
    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 60) gs.field_w = 80;
    if (gs.field_h < 20) gs.field_h = 24;

    int status_row = gs.field_h;
    gs.player_row = gs.field_h - 2;

    game_rng_seed();

    game_screen_enter(gs.field_h - 1);

    bool quit = false;

    while (!quit) {
        // --- New game setup ---
        gs.lives = 3;
        gs.score = 0;
        gs.level = 1;
        gs.player_x = gs.field_w / 2;

        init_enemies(&gs);
        init_barriers(&gs);
        init_bullets(&gs);

        int tick_ms = TICK_MS_INIT;
        bool game_over = false;

        printf("\x1b[2J");  // clear screen

        // --- Game loop ---
        while (!game_over) {
            uint32_t tick_start = time_us_32();

            // --- Input ---
            char c;
            while (rx_fifo_try_get(&c)) {
                if (c == 'q' || c == 'Q') { game_over = true; quit = true; break; }
                if (c == ' ') fire_bullet(&gs);
                if (c == 0x1b) {
                    // Arrow key sequence
                    char seq1, seq2;
                    if (rx_fifo_try_get(&seq1) && seq1 == '[') {
                        if (rx_fifo_try_get(&seq2)) {
                            if (seq2 == 'C') { // right
                                gs.player_x += 2;
                                if (gs.player_x > gs.field_w - 1) gs.player_x = gs.field_w - 1;
                            }
                            if (seq2 == 'D') { // left
                                gs.player_x -= 2;
                                if (gs.player_x < 2) gs.player_x = 2;
                            }
                        }
                    }
                }
            }
            if (quit) break;

            // --- Move bullets up ---
            move_bullets(&gs);

            // --- Move bombs down ---
            move_bombs(&gs);

            // --- Move fleet ---
            move_fleet(&gs);

            // --- Drop new bombs ---
            drop_bombs(&gs);

            // --- Collision: bullets vs enemies ---
            check_bullet_hits(&gs);

            // --- Collision: bullets vs barriers ---
            check_bullet_barrier(&gs);

            // --- Collision: bombs vs barriers ---
            check_bomb_barrier(&gs);

            // --- Collision: bombs vs player ---
            if (check_bomb_player(&gs)) {
                gs.lives--;
                if (gs.lives <= 0) {
                    game_over = true;
                }
                // Flash player hit
                ui_term_cursor_position(gs.player_row, gs.player_x - 1);
                printf("%s*X*%s", red, rst);
                tx_fifo_wait_drain();
                busy_wait_ms(300);
            }

            // --- Enemies reached bottom = instant death ---
            if (enemies_reached_bottom(&gs)) {
                gs.lives = 0;
                game_over = true;
            }

            // --- Wave cleared? ---
            if (gs.enemies_alive <= 0) {
                gs.level++;
                // Speed up
                tick_ms -= 5;
                if (tick_ms < TICK_MS_MIN) tick_ms = TICK_MS_MIN;
                // Clear screen and start new wave
                printf("\x1b[2J");
                ui_term_cursor_position(gs.field_h / 2, (gs.field_w - 14) / 2);
                printf("%s=== WAVE %d ===%s", grn, gs.level, rst);
                tx_fifo_wait_drain();
                busy_wait_ms(1000);
                printf("\x1b[2J");
                init_enemies(&gs);
                init_barriers(&gs);
                init_bullets(&gs);
            }

            // --- Draw ---
            tx_fifo_wait_drain();
            draw_frame(&gs, status_row);

            // --- Timing ---
            uint32_t elapsed = (time_us_32() - tick_start) / 1000;
            if ((int)elapsed < tick_ms) {
                busy_wait_ms(tick_ms - (int)elapsed);
            }
        }

        // --- Game over ---
        if (gs.score > gs.hi_score) gs.hi_score = gs.score;

        if (!quit) {
            // Show game over screen
            int mid_r = gs.field_h / 2;
            int mid_c = (gs.field_w - 30) / 2;
            if (mid_c < 1) mid_c = 1;

            ui_term_cursor_position(mid_r - 2, mid_c);
            printf("%s╔══════════════════════════╗%s", red, rst);
            ui_term_cursor_position(mid_r - 1, mid_c);
            printf("%s║   PROTOCOL BREACH !!!    ║%s", red, rst);
            ui_term_cursor_position(mid_r, mid_c);
            printf("%s║  Bus compromised. RESET  ║%s", red, rst);
            ui_term_cursor_position(mid_r + 1, mid_c);
            printf("%s║    Score: %s%-6d%s         ║%s", red, ylw, gs.score, red, rst);
            ui_term_cursor_position(mid_r + 2, mid_c);
            printf("%s╚══════════════════════════╝%s", red, rst);
            ui_term_cursor_position(mid_r + 4, (gs.field_w - 28) / 2);
            printf("SPACE = play again   q = quit");
            tx_fifo_wait_drain();

            // Wait for input
            bool waiting = true;
            while (waiting) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; waiting = false; }
                    if (c == ' ') { waiting = false; printf("\x1b[2J"); }
                }
                busy_wait_ms(50);
            }
        }
    }

    game_screen_exit();
}
