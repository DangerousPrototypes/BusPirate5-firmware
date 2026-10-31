// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file stkover.c
 * @brief Stack Overflow — vertical jump platformer for Bus Pirate.
 *
 * Jump upward from platform to platform on an endless vertical PCB.
 * Platforms are electronics components: IC packages, breadboard rows,
 * PCB layers, caps, resistors, USB connectors, SOT packages, etc.
 * Screen scrolls up as you rise. Fall off the bottom = game over.
 *
 * Controls: LEFT/RIGHT = move, jump is automatic on landing.
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
#include "stkover.h"

static const char* const usage[] = {
    "stkover",
    "Launch Stack Overflow:%s stkover",
    "",
    "Jump up platforms on an endless vertical PCB!",
    "LEFT/RIGHT=move  Auto-jump on landing  q=quit",
};

const bp_command_def_t stkover_def = {
    .name = "stkover",
    .description = T_HELP_STKOVER,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MAX_PLATFORMS   40      // max platforms in world at once
#define GRAVITY         1      // downward accel per tick (x4 fixed-point)
#define JUMP_VY         9      // upward velocity on bounce (x4 fixed-point)
                                // peak height = sum(9..1)/4 = 45/4 ≈ 11 rows
#define SPRING_VY       13     // spring platform bounce (x4)
#define MOVE_SPEED      3      // horizontal speed per input tick
#define TICK_MS         75     // ms per frame (slower = more playable)
#define MAX_HORIZ_DIST  22     // max horizontal distance between platform centers

// Platform visual types — ASCII schematic symbols from pcbrun.c
// art[0] = top row (landing surface), art[1..2] = body below
#define MAX_ART_H  3

typedef struct {
    uint8_t w;                   // width in chars
    uint8_t h;                   // height in rows
    const char* art[MAX_ART_H];  // rows, [0]=top
} plat_style_t;

static const plat_style_t plat_styles[] = {
    { 6, 1, { "-[//]-", NULL,      NULL     } },  //  0 Resistor
    { 4, 1, { "-||-",   NULL,      NULL     } },  //  1 Capacitor
    { 5, 1, { "-|>|-",  NULL,      NULL     } },  //  2 Diode
    { 8, 2, { "[DIP-16]","[______]",NULL    } },  //  3 DIP IC
    { 4, 1, { "-|[-",   NULL,      NULL     } },  //  4 Elec Cap
    { 6, 1, { "-|[]|-", NULL,      NULL     } },  //  5 Crystal
    { 5, 2, { " FET ",  "[|||]",   NULL     } },  //  6 MOSFET
    { 6, 3, { "/\\/\\/\\","||||||","[====]" } },  //  7 Heatsink
    { 5, 1, { "-|>]-",  NULL,      NULL     } },  //  8 LED
    { 4, 1, { "-//-",   NULL,      NULL     } },  //  9 Fuse
    { 6, 1, { "-^^^^-", NULL,      NULL     } },  // 10 Inductor
    { 7, 3, { "[RELAY]","| o-o |","[_____]"} },  // 11 Relay
    { 4, 1, { "=|>-",   NULL,      NULL     } },  // 12 Op-amp
    { 7, 1, { "-|<z>|-",NULL,      NULL     } },  // 13 Zener
    { 4, 1, { "- /-",   NULL,      NULL     } },  // 14 Switch
};
#define PTYPE_COUNT (sizeof(plat_styles) / sizeof(plat_styles[0]))

// Special platform: fragile (breaks after landing)
#define PFLAG_NORMAL    0
#define PFLAG_FRAGILE   1   // crumbles after 1 bounce
#define PFLAG_MOVING    2   // moves side to side
#define PFLAG_SPRING    3   // extra high bounce

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
    int16_t x;              // left edge, world x
    int16_t y;              // world y (increases upward)
    uint8_t w;              // width in columns
    uint8_t ptype;          // visual type
    int8_t  flag;           // behavior flag
    int8_t  move_dir;       // for PFLAG_MOVING: +1 or -1
    bool    alive;          // still exists?
} platform_t;

typedef struct {
    int field_w;            // terminal columns
    int field_h;            // terminal rows
    int plat_count;
    int px;                 // player x (world, 1-based columns)
    int py4;                // player y * 4 (fixed point)
    int vy4;                // player velocity * 4 (positive = up)
    int prev_py4;           // previous py4 for sweep landing detection
    int camera_y;           // bottom of visible world window
    int max_height;         // highest y reached (for scoring)
    int score;
    int hi_score;
    int lives;
    int bounce_flash;       // frames remaining for bounce glow (0=off)
    int bounce_plat;        // index of platform that was just bounced on
    int status_row;
    int play_top;           // topmost play row (1)
    int play_bot;           // bottommost play row
    platform_t platforms[MAX_PLATFORMS];
} stkover_state_t;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int world_to_screen_row(stkover_state_t* gs, int wy) {
    return gs->play_bot - (wy - gs->camera_y);
}

static int screen_to_world_y(stkover_state_t* gs, int row) {
    return gs->camera_y + (gs->play_bot - row);
}

static void spawn_platform(stkover_state_t* gs, int wy) {
    if (gs->plat_count >= MAX_PLATFORMS) return;
    platform_t* p = &gs->platforms[gs->plat_count++];
    p->y = (int16_t)wy;
    p->ptype = (uint8_t)(game_rng_next() % PTYPE_COUNT);
    p->w = plat_styles[p->ptype].w;

    // Find nearest platform below for horizontal constraint
    int ref_cx = gs->field_w / 2;
    int closest_dy = 9999;
    for (int i = 0; i < gs->plat_count - 1; i++) {
        if (!gs->platforms[i].alive) continue;
        int dy = wy - gs->platforms[i].y;
        if (dy > 0 && dy < closest_dy) {
            closest_dy = dy;
            ref_cx = gs->platforms[i].x + gs->platforms[i].w / 2;
        }
    }
    int min_x = ref_cx - MAX_HORIZ_DIST - p->w / 2;
    int max_x = ref_cx + MAX_HORIZ_DIST - p->w / 2;
    if (min_x < 2) min_x = 2;
    if (max_x > gs->field_w - p->w - 1) max_x = gs->field_w - p->w - 1;
    if (min_x > max_x) min_x = max_x;
    int range = max_x - min_x + 1;
    p->x = (int16_t)(min_x + (int)(game_rng_next() % (range > 0 ? range : 1)));

    p->ptype = (uint8_t)(game_rng_next() % PTYPE_COUNT);
    p->alive = true;
    p->move_dir = 1;

    int r = (int)(game_rng_next() % 100);
    if (wy > 30 && r < 12) {
        p->flag = PFLAG_FRAGILE;
    } else if (wy > 60 && r < 22) {
        p->flag = PFLAG_MOVING;
    } else if (r < 8) {
        p->flag = PFLAG_SPRING;
    } else {
        p->flag = PFLAG_NORMAL;
    }
}

static void init_platforms(stkover_state_t* gs) {
    gs->plat_count = 0;
    gs->platforms[0].x = 1;
    gs->platforms[0].y = 0;
    gs->platforms[0].w = (uint8_t)(gs->field_w - 2);
    gs->platforms[0].ptype = 0;
    gs->platforms[0].flag = PFLAG_NORMAL;
    gs->platforms[0].alive = true;
    gs->platforms[0].move_dir = 1;
    gs->plat_count = 1;

    int gen_top = gs->field_h + 10;
    for (int wy = 4; wy < gen_top; wy += 3 + (int)(game_rng_next() % 3)) {
        spawn_platform(gs, wy);
    }
}

// Generate new platforms above the camera as player ascends
static int highest_platform_y(stkover_state_t* gs) {
    int top = 0;
    for (int i = 0; i < gs->plat_count; i++) {
        if (gs->platforms[i].alive && gs->platforms[i].y > top)
            top = gs->platforms[i].y;
    }
    return top;
}

static void generate_upward(stkover_state_t* gs) {
    int vis_top = gs->camera_y + (gs->play_bot - gs->play_top) + 5;
    int htop = highest_platform_y(gs);
    while (htop < vis_top) {
        int gap = 3 + (int)(game_rng_next() % 3);
        htop += gap;
        spawn_platform(gs, htop);
    }
}

// Remove platforms far below camera
static void cull_platforms(stkover_state_t* gs) {
    int cull_y = gs->camera_y - 10;
    for (int i = 1; i < gs->plat_count; i++) {
        if (gs->platforms[i].y < cull_y) {
            gs->platforms[i] = gs->platforms[gs->plat_count - 1];
            gs->plat_count--;
            i--;
        }
    }
}

// Move moving platforms
static void update_moving_platforms(stkover_state_t* gs) {
    for (int i = 0; i < gs->plat_count; i++) {
        if (!gs->platforms[i].alive || gs->platforms[i].flag != PFLAG_MOVING) continue;
        gs->platforms[i].x += gs->platforms[i].move_dir;
        if (gs->platforms[i].x + gs->platforms[i].w > gs->field_w) {
            gs->platforms[i].move_dir = -1;
        }
        if (gs->platforms[i].x < 1) {
            gs->platforms[i].move_dir = 1;
        }
    }
}

// Check if player passed through a platform while falling (sweep test).
// Uses prev_py4 and py4 to catch fast-moving player.
static int check_landing(stkover_state_t* gs) {
    if (gs->vy4 > 0) return -1;
    int cur_y = gs->py4 / 4;
    int prev_y = gs->prev_py4 / 4;
    int top_y = cur_y;
    int bot_y = prev_y;
    if (top_y > bot_y) { int t = top_y; top_y = bot_y; bot_y = t; }

    int best = -1;
    int best_y = -9999;
    for (int i = 0; i < gs->plat_count; i++) {
        if (!gs->platforms[i].alive) continue;
        int py = gs->platforms[i].y;
        if (py < gs->camera_y) continue;
        if (py < top_y || py > bot_y) continue;
        int pleft = gs->px - 1;
        int pright = gs->px + 1;
        if (pright < gs->platforms[i].x || pleft > gs->platforms[i].x + gs->platforms[i].w - 1)
            continue;
        if (py > best_y) {
            best_y = py;
            best = i;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Drawing — line-buffer rendering (no flicker)
// ---------------------------------------------------------------------------

// Color palette indices
#define CLR_0   0
#define CLR_GRN 1
#define CLR_RED 2
#define CLR_YLW 3
#define CLR_CYN 4
#define CLR_WHT 5
#define CLR_MAG 6
#define CLR_DIM 7

static const char* const clr_lut[] = {
    "\x1b[0m", "\x1b[32m", "\x1b[31m", "\x1b[33m",
    "\x1b[36m", "\x1b[1;37m", "\x1b[35m", "\x1b[2m",
};

#define ROW_BUF_W 120

// Emit one composited row — no flicker (overwrite, never clear first)
static void emit_row(int row, const char* rbuf, const uint8_t* rclr, int w) {
    bool color = system_config.terminal_ansi_color;
    ui_term_cursor_position(row, 1);
    if (color) {
        uint8_t cur = 255;
        int span_start = 0;
        for (int i = 0; i < w; i++) {
            if (rclr[i] != cur) {
                if (i > span_start) {
                    printf("%.*s", i - span_start, &rbuf[span_start]);
                }
                cur = rclr[i];
                printf("%s", clr_lut[cur]);
                span_start = i;
            }
        }
        if (w > span_start) {
            printf("%.*s", w - span_start, &rbuf[span_start]);
        }
        printf("\x1b[0m");
    } else {
        printf("%.*s", w, rbuf);
    }
    printf("\x1b[K");
}

static void draw_frame(stkover_state_t* gs) {
    int w = gs->field_w;
    if (w > ROW_BUF_W) w = ROW_BUF_W;
    char rbuf[ROW_BUF_W];
    uint8_t rclr[ROW_BUF_W];
    int pr = world_to_screen_row(gs, gs->py4 / 4);
    const char* pch = (gs->vy4 > 0) ? "/^\\" : "\\_/";

    for (int row = gs->play_top; row <= gs->play_bot; row++) {
        memset(rbuf, ' ', w);
        memset(rclr, CLR_0, w);

        rbuf[0] = '|';
        rclr[0] = CLR_DIM;

        int wy = screen_to_world_y(gs, row);
        if (wy >= 0 && wy % 10 == 0 && w >= 8) {
            char hbuf[8];
            int n = snprintf(hbuf, sizeof(hbuf), "%3d", wy);
            int start = w - 5;
            for (int k = 0; k < n && start + k < w; k++) {
                if (start + k > 0) {
                    rbuf[start + k] = hbuf[k];
                    rclr[start + k] = CLR_DIM;
                }
            }
        }

        // --- Stamp platforms ---
        for (int i = 0; i < gs->plat_count; i++) {
            if (!gs->platforms[i].alive) continue;

            uint8_t clr;
            if (gs->bounce_flash > 0 && i == gs->bounce_plat) {
                clr = CLR_WHT;
            } else {
                switch (gs->platforms[i].flag) {
                    case PFLAG_FRAGILE: clr = CLR_RED; break;
                    case PFLAG_MOVING:  clr = CLR_YLW; break;
                    case PFLAG_SPRING:  clr = CLR_MAG; break;
                    default:            clr = CLR_CYN; break;
                }
            }

            if (i == 0) {
                int sr = world_to_screen_row(gs, gs->platforms[0].y);
                if (sr == row) {
                    const char* pat = "=[===PCB===]";
                    int plen = 12;
                    uint8_t gclr = (gs->bounce_flash > 0 && gs->bounce_plat == 0) ? CLR_WHT : CLR_GRN;
                    for (int pos = 1; pos < w - 1; pos++) {
                        rbuf[pos] = pat[(pos - 1) % plen];
                        rclr[pos] = gclr;
                    }
                }
                continue;
            }

            const plat_style_t* st = &plat_styles[gs->platforms[i].ptype];

            for (int r = 0; r < st->h; r++) {
                if (st->art[r] == NULL) continue;
                int sr = world_to_screen_row(gs, gs->platforms[i].y - r);
                if (sr != row) continue;
                int sx = gs->platforms[i].x - 1;
                if (sx < 1) sx = 1;
                int alen = strlen(st->art[r]);
                for (int k = 0; k < alen; k++) {
                    int pos = sx + k;
                    if (pos >= 1 && pos < w - 1) {
                        rbuf[pos] = st->art[r][k];
                        rclr[pos] = clr;
                    }
                }
            }
        }

        if (row == pr && gs->px >= 2 && gs->px <= gs->field_w - 1) {
            int pos = gs->px - 2;
            for (int k = 0; k < 3; k++) {
                if (pos + k >= 1 && pos + k < w - 1) {
                    rbuf[pos + k] = pch[k];
                    rclr[pos + k] = CLR_WHT;
                }
            }
        }

        emit_row(row, rbuf, rclr, w);
    }

    // Status bar
    ui_term_cursor_position(gs->status_row, 1);
    printf(" %sSTK OVER%s S:%s%05d%s Hi:%s%05d%s Ht:%s%4d%s x%s%d%s </>=move q=quit",
           wht, rst, ylw, gs->score, rst, red, gs->hi_score, rst,
           cyn, gs->max_height, rst, grn, gs->lives, rst);
    printf("\x1b[K");
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void stkover_handler(struct command_result* res) {
    res->error = false;

    stkover_state_t gs = {0};
    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 40) gs.field_w = 80;
    if (gs.field_h < 16) gs.field_h = 24;

    gs.play_top = 1;
    gs.play_bot = gs.field_h - 2;
    gs.status_row = gs.field_h;

    game_rng_seed();

    game_screen_enter(gs.field_h - 1);

    bool quit = false;

    while (!quit) {
        gs.px = gs.field_w / 2;
        gs.py4 = 1 * 4;
        gs.prev_py4 = gs.py4;
        gs.vy4 = JUMP_VY;
        gs.camera_y = 0;
        gs.max_height = 0;
        gs.score = 0;
        gs.lives = 1;

        gs.bounce_flash = 0;
        gs.bounce_plat = -1;
        init_platforms(&gs);
        printf("\x1b[2J");

        ui_term_cursor_position(gs.field_h / 2 - 1, (gs.field_w - 24) / 2);
        printf("%s=== STACK OVERFLOW ===%s", cyn, rst);
        ui_term_cursor_position(gs.field_h / 2 + 0, (gs.field_w - 24) / 2);
        printf("  Jump the components!");
        ui_term_cursor_position(gs.field_h / 2 + 1, (gs.field_w - 24) / 2);
        printf("  LEFT/RIGHT to move");
        tx_fifo_wait_drain();
        busy_wait_ms(1500);
        printf("\x1b[2J");

        bool game_over = false;
        int move_input = 0; // -1 left, 0 none, +1 right

        while (!game_over) {
            uint32_t tick_start = time_us_32();

            // --- Input ---
            move_input = 0;
            char c;
            while (rx_fifo_try_get(&c)) {
                if (c == 'q' || c == 'Q') { game_over = true; quit = true; break; }
                if (c == 0x1b) {
                    char s1, s2;
                    if (rx_fifo_try_get(&s1) && s1 == '[') {
                        if (rx_fifo_try_get(&s2)) {
                            if (s2 == 'C') move_input = 1;  // right
                            if (s2 == 'D') move_input = -1; // left
                        }
                    }
                }
            }
            if (quit) break;

            // --- Horizontal movement ---
            gs.px += move_input * MOVE_SPEED;
            if (gs.px < 1) gs.px = gs.field_w - 1;
            if (gs.px > gs.field_w) gs.px = 2;

            // --- Physics ---
            gs.prev_py4 = gs.py4;
            gs.vy4 -= GRAVITY;
            gs.py4 += gs.vy4;

            // --- Landing check ---
            if (gs.vy4 <= 0) {
                int landed = check_landing(&gs);
                if (landed >= 0) {
                    platform_t* p = &gs.platforms[landed];
                    gs.py4 = p->y * 4;

                    if (p->flag == PFLAG_SPRING) {
                        gs.vy4 = SPRING_VY;
                    } else {
                        gs.vy4 = JUMP_VY;
                    }

                    gs.bounce_flash = 3;
                    gs.bounce_plat = landed;

                    if (p->flag == PFLAG_FRAGILE) {
                        p->alive = false;
                    }
                }
            }

            // --- Update camera ---
            int py = gs.py4 / 4;
            int screen_margin = (gs.play_bot - gs.play_top) / 3;
            if (py - gs.camera_y > (gs.play_bot - gs.play_top) - screen_margin) {
                gs.camera_y = py - (gs.play_bot - gs.play_top) + screen_margin;
            }

            // --- Track max height / score ---
            if (py > gs.max_height) {
                gs.score += (py - gs.max_height);
                gs.max_height = py;
            }

            generate_upward(&gs);
            update_moving_platforms(&gs);
            cull_platforms(&gs);

            if (gs.bounce_flash > 0) gs.bounce_flash--;

            tx_fifo_wait_drain();
            draw_frame(&gs);

            if (py < gs.camera_y - 1) {
                game_over = true;
            }

            // --- Timing ---
            uint32_t elapsed = (time_us_32() - tick_start) / 1000;
            if ((int)elapsed < TICK_MS) {
                busy_wait_ms(TICK_MS - (int)elapsed);
            }
        }

        // --- Game Over ---
        if (gs.score > gs.hi_score) gs.hi_score = gs.score;

        if (!quit) {
            int mr = gs.field_h / 2;
            int mc = (gs.field_w - 28) / 2;
            if (mc < 1) mc = 1;

            for (int r = mr - 2; r <= mr + 5; r++) {
                ui_term_cursor_position(r, 1);
                printf("\x1b[K");
            }

            ui_term_cursor_position(mr - 2, mc);
            printf("%s+==========================+%s", red, rst);
            ui_term_cursor_position(mr - 1, mc);
            printf("%s|    STACK OVERFLOW !!!    |%s", red, rst);
            ui_term_cursor_position(mr, mc);
            printf("%s|  Missed the platform and |%s", red, rst);
            ui_term_cursor_position(mr + 1, mc);
            printf("%s|  fell off the bottom!    |%s", red, rst);
            ui_term_cursor_position(mr + 2, mc);
            printf("%s|  Ht: %s%06d%s  S: %s%06d%s   |%s", red, ylw, gs.max_height, red, ylw, gs.score, red, rst);
            ui_term_cursor_position(mr + 3, mc);
            printf("%s+==========================+%s", red, rst);
            ui_term_cursor_position(mr + 5, (gs.field_w - 28) / 2);
            printf("SPACE = try again   q = quit");
            tx_fifo_wait_drain();

            bool waiting = true;
            while (waiting) {
                char ch;
                if (rx_fifo_try_get(&ch)) {
                    if (ch == 'q' || ch == 'Q') { quit = true; waiting = false; }
                    if (ch == ' ') { waiting = false; printf("\x1b[2J"); }
                }
                busy_wait_ms(50);
            }
        }
    }

    game_screen_exit();
}
