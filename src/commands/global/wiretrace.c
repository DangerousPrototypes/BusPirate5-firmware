// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file wiretrace.c
 * @brief Wire Trace — Pac-Man-style PCB maze game for Bus Pirate.
 *
 * Navigate your probe (P) from start (S) to goal (G) through a PCB maze.
 * Avoid the blue smoke monsters (~/* — escaped magic smoke!) that patrol
 * the corridors. Collect solder blobs (o) for bonus points. Step on
 * vias (()) to teleport to a paired via elsewhere on the board.
 *
 * Copper trace walls form the maze — multiple paths exist, plan your route!
 * Timer counts down each level. Sparks get faster, maze gets bigger.
 * 3 lives per game. Lose a life when a spark catches you.
 *
 * Controls: WASD / arrow keys = move, q = quit.
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
#include "wiretrace.h"

static const char* const usage[] = {
    "trace",
    "Launch Wire Trace:%s trace",
    "",
    "Pac-Man-style PCB maze: dodge the blue smoke!",
    "WASD/arrows=move  q=quit",
};

const bp_command_def_t wiretrace_def = {
    .name = "trace",
    .description = T_HELP_WIRETRACE,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define WT_MAX_W    30
#define WT_MAX_H    20
#define MAX_SPARKS  4
#define MAX_VIAS    3       // max via pairs
#define TICK_MS     120     // ms per game frame

// Cell types
#define C_FLOOR     0       // walkable FR4 substrate
#define C_WALL      1       // copper trace (impassable)
#define C_DOT       2       // solder blob collectible
#define C_VIA       3       // via teleporter
#define C_START     4       // start test point
#define C_GOAL      5       // goal test point

// Color LUT indices
#define CLR_FLOOR   0
#define CLR_WALL    1
#define CLR_DOT     2
#define CLR_VIA     3
#define CLR_START   4
#define CLR_GOAL    5
#define CLR_PLAYER  6
#define CLR_SPARK   7

// ---------------------------------------------------------------------------
// ANSI helpers
// ---------------------------------------------------------------------------
static const char* CC(const char* c) { return system_config.terminal_ansi_color ? c : ""; }
static const char* RR(void) { return system_config.terminal_ansi_color ? "\x1b[0m" : ""; }

// Every entry starts with \x1b[0; to RESET all attributes first.
// This fixes color bleed where dim dots inherited adjacent trace colors.
static const char* const clr_lut[] = {
    "\x1b[0;2;32m",        // 0: floor — dim green (FR4 substrate)
    "\x1b[0;43m",           // 1: wall  — yellow bg (copper fill)
    "\x1b[0;37m",           // 2: dot   — grey/silver (solder blob)
    "\x1b[0;1;36m",         // 3: via   — bright cyan
    "\x1b[0;32m",           // 4: start — green
    "\x1b[0;1;37m",         // 5: goal  — bright white
    "\x1b[0;1;32m",         // 6: player — bright green
    "\x1b[0;2;36m",         // 7: spark — dim cyan (blue-grey magic smoke)
};

// ---------------------------------------------------------------------------
// Sub-structs
// ---------------------------------------------------------------------------
typedef struct { uint8_t r1, c1, r2, c2; } via_pair_t;
typedef struct {
    uint8_t r, c;
    uint8_t dir;                // 0=up, 1=down, 2=left, 3=right
} spark_t;

// ---------------------------------------------------------------------------
// Game state (all on stack in handler, passed by pointer)
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t board[WT_MAX_H][WT_MAX_W];
    uint16_t temp_buf[WT_MAX_H * WT_MAX_W]; // shared: maze stack + BFS queue
    int bw, bh;
    int player_r, player_c;
    int start_r, start_c;
    int goal_r, goal_c;
    int level, score, hi_score;
    int lives;
    int dots_collected, dots_total;
    int field_w, field_h;
    int frame_count;
    int invuln;                 // invulnerability frames after respawn/teleport
    via_pair_t via_pairs[MAX_VIAS];
    int via_count;
    spark_t sparks[MAX_SPARKS];
    int spark_count;
    int spark_speed;            // frames between spark moves
} wt_state_t;

// Directions: up, down, left, right
static const int dr[] = { -1, 1, 0, 0 };
static const int dc[] = { 0, 0, -1, 1 };

static bool in_bounds(wt_state_t* gs, int r, int c) {
    return r >= 0 && r < gs->bh && c >= 0 && c < gs->bw;
}

// ---------------------------------------------------------------------------
// Maze generation — recursive backtracker then open extra passages for loops
// ---------------------------------------------------------------------------

static void carve_maze(wt_state_t* gs) {
    uint16_t* stack = gs->temp_buf;
    int sp = 0;

    gs->board[1][1] = C_FLOOR;
    stack[0] = (1 << 8) | 1;
    sp = 1;

    while (sp > 0) {
        int cr = stack[sp - 1] >> 8;
        int cc = stack[sp - 1] & 0xFF;

        int dirs[4] = {0, 1, 2, 3};
        for (int i = 3; i > 0; i--) {
            int j = (int)(game_rng_next() % (i + 1));
            int t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
        }

        bool found = false;
        for (int d = 0; d < 4; d++) {
            int nr = cr + dr[dirs[d]] * 2;
            int nc = cc + dc[dirs[d]] * 2;
            if (in_bounds(gs, nr, nc) && gs->board[nr][nc] != C_FLOOR) {
                gs->board[cr + dr[dirs[d]]][cc + dc[dirs[d]]] = C_FLOOR;
                gs->board[nr][nc] = C_FLOOR;
                if (sp < WT_MAX_H * WT_MAX_W) {
                    stack[sp++] = (uint16_t)((nr << 8) | nc);
                }
                found = true;
                break;
            }
        }
        if (!found) sp--;
    }
}

// Open random internal walls to create loops (multiple paths, Pac-Man style)
static void open_passages(wt_state_t* gs, int pct) {
    for (int r = 1; r < gs->bh - 1; r++) {
        for (int c = 1; c < gs->bw - 1; c++) {
            if (gs->board[r][c] != C_WALL) continue;
            // Only open walls that separate two floor cells (creates a shortcut)
            bool h = (c > 0 && c < gs->bw - 1 &&
                      gs->board[r][c - 1] == C_FLOOR && gs->board[r][c + 1] == C_FLOOR);
            bool v = (r > 0 && r < gs->bh - 1 &&
                      gs->board[r - 1][c] == C_FLOOR && gs->board[r + 1][c] == C_FLOOR);
            if ((h || v) && (int)(game_rng_next() % 100) < pct) {
                gs->board[r][c] = C_FLOOR;
            }
        }
    }
}

// BFS path check (uses shared temp_buf)
static bool path_exists(wt_state_t* gs, int sr_v, int sc_v, int gr_v, int gc_v) {
    #define VIS_BIT 0x80
    uint16_t* queue = gs->temp_buf;
    int head = 0, tail = 0;

    queue[tail++] = (uint16_t)((sr_v << 8) | sc_v);
    gs->board[sr_v][sc_v] |= VIS_BIT;
    bool found = false;

    while (head < tail) {
        int cr = queue[head] >> 8;
        int cc = queue[head] & 0xFF;
        head++;
        if (cr == gr_v && cc == gc_v) { found = true; break; }

        for (int d = 0; d < 4; d++) {
            int nr = cr + dr[d];
            int nc = cc + dc[d];
            if (!in_bounds(gs, nr, nc) || (gs->board[nr][nc] & VIS_BIT)) continue;
            uint8_t cell = gs->board[nr][nc] & 0x7F;
            if (cell != C_WALL) {
                gs->board[nr][nc] |= VIS_BIT;
                queue[tail++] = (uint16_t)((nr << 8) | nc);
            }
        }
    }

    for (int r = 0; r < gs->bh; r++)
        for (int c = 0; c < gs->bw; c++)
            gs->board[r][c] &= 0x7F;

    return found;
    #undef VIS_BIT
}

// ---------------------------------------------------------------------------
// Board setup helpers
// ---------------------------------------------------------------------------

static void find_walkable(wt_state_t* gs, int pref_r, int pref_c, int* out_r, int* out_c) {
    for (int dist = 0; dist < gs->bw + gs->bh; dist++) {
        for (int dr2 = -dist; dr2 <= dist; dr2++) {
            for (int dc2 = -dist; dc2 <= dist; dc2++) {
                if (abs(dr2) + abs(dc2) != dist) continue;
                int r = pref_r + dr2;
                int c = pref_c + dc2;
                if (in_bounds(gs, r, c) && gs->board[r][c] == C_FLOOR) {
                    *out_r = r;
                    *out_c = c;
                    return;
                }
            }
        }
    }
    *out_r = 1;
    *out_c = 1;
}

// Scatter solder blob collectibles on floor cells
static int place_dots(wt_state_t* gs, int pct) {
    int count = 0;
    for (int r = 1; r < gs->bh - 1; r++) {
        for (int c = 1; c < gs->bw - 1; c++) {
            if (gs->board[r][c] == C_FLOOR && (int)(game_rng_next() % 100) < pct) {
                gs->board[r][c] = C_DOT;
                count++;
            }
        }
    }
    return count;
}

// Place via teleporter pairs (far apart, on floor cells)
static void place_vias(wt_state_t* gs, int count) {
    gs->via_count = 0;
    for (int i = 0; i < count && gs->via_count < MAX_VIAS; i++) {
        int r1, c1, r2, c2;
        int attempts = 0;
        do {
            r1 = 1 + (int)(game_rng_next() % (gs->bh - 2));
            c1 = 1 + (int)(game_rng_next() % (gs->bw - 2));
            r2 = 1 + (int)(game_rng_next() % (gs->bh - 2));
            c2 = 1 + (int)(game_rng_next() % (gs->bw - 2));
            attempts++;
        } while (attempts < 100 &&
                 (gs->board[r1][c1] != C_FLOOR || gs->board[r2][c2] != C_FLOOR ||
                  abs(r1 - r2) + abs(c1 - c2) < 5 ||
                  (r1 == r2 && c1 == c2)));

        if (gs->board[r1][c1] == C_FLOOR && gs->board[r2][c2] == C_FLOOR) {
            gs->board[r1][c1] = C_VIA;
            gs->board[r2][c2] = C_VIA;
            gs->via_pairs[gs->via_count].r1 = (uint8_t)r1;
            gs->via_pairs[gs->via_count].c1 = (uint8_t)c1;
            gs->via_pairs[gs->via_count].r2 = (uint8_t)r2;
            gs->via_pairs[gs->via_count].c2 = (uint8_t)c2;
            gs->via_count++;
        }
    }
}

// ---------------------------------------------------------------------------
// Spark enemies (escaped magic smoke)
// ---------------------------------------------------------------------------

static void init_sparks(wt_state_t* gs, int count) {
    gs->spark_count = count;
    if (gs->spark_count > MAX_SPARKS) gs->spark_count = MAX_SPARKS;
    for (int i = 0; i < gs->spark_count; i++) {
        int attempts = 0;
        do {
            gs->sparks[i].r = (uint8_t)(1 + (int)(game_rng_next() % (gs->bh - 2)));
            gs->sparks[i].c = (uint8_t)(1 + (int)(game_rng_next() % (gs->bw - 2)));
            attempts++;
        } while (attempts < 100 &&
                 (gs->board[gs->sparks[i].r][gs->sparks[i].c] == C_WALL ||
                  (abs((int)gs->sparks[i].r - gs->start_r) + abs((int)gs->sparks[i].c - gs->start_c) < 6)));
        gs->sparks[i].dir = (uint8_t)(game_rng_next() % 4);
    }
}

static bool spark_walkable(wt_state_t* gs, int r, int c) {
    if (!in_bounds(gs, r, c)) return false;
    return gs->board[r][c] != C_WALL;
}

static void move_sparks(wt_state_t* gs) {
    for (int i = 0; i < gs->spark_count; i++) {
        int nr = (int)gs->sparks[i].r + dr[gs->sparks[i].dir];
        int nc = (int)gs->sparks[i].c + dc[gs->sparks[i].dir];

        bool wall_ahead = !spark_walkable(gs, nr, nc);

        // Reverse direction index: 0<->1 (up<->down), 2<->3 (left<->right)
        int reverse = gs->sparks[i].dir ^ 1;

        // Collect valid directions
        int valid[4];
        int nv = 0;
        for (int d = 0; d < 4; d++) {
            int tr = (int)gs->sparks[i].r + dr[d];
            int tc = (int)gs->sparks[i].c + dc[d];
            if (spark_walkable(gs, tr, tc)) {
                valid[nv++] = d;
            }
        }

        bool at_intersection = (nv > 2);

        // Change direction if wall ahead, or randomly at intersections
        if (wall_ahead || (at_intersection && (int)(game_rng_next() % 100) < 30)) {
            if (nv > 0) {
                // Prefer not reversing
                int non_rev[4];
                int nnr = 0;
                for (int j = 0; j < nv; j++) {
                    if (valid[j] != reverse) non_rev[nnr++] = valid[j];
                }

                if (nnr > 0) {
                    // Increasingly attracted to player with each level
                    int chase_pct = gs->level * 10;
                    if (chase_pct > 80) chase_pct = 80;
                    if ((int)(game_rng_next() % 100) < chase_pct) {
                        int best = non_rev[0];
                        int best_dist = 9999;
                        for (int j = 0; j < nnr; j++) {
                            int tr = (int)gs->sparks[i].r + dr[non_rev[j]];
                            int tc = (int)gs->sparks[i].c + dc[non_rev[j]];
                            int dist = abs(tr - gs->player_r) + abs(tc - gs->player_c);
                            if (dist < best_dist) { best_dist = dist; best = non_rev[j]; }
                        }
                        gs->sparks[i].dir = (uint8_t)best;
                    } else {
                        gs->sparks[i].dir = (uint8_t)non_rev[game_rng_next() % nnr];
                    }
                } else if (nv > 0) {
                    gs->sparks[i].dir = (uint8_t)valid[game_rng_next() % nv];
                }
            }
        }

        nr = (int)gs->sparks[i].r + dr[gs->sparks[i].dir];
        nc = (int)gs->sparks[i].c + dc[gs->sparks[i].dir];
        if (spark_walkable(gs, nr, nc)) {
            gs->sparks[i].r = (uint8_t)nr;
            gs->sparks[i].c = (uint8_t)nc;
        }
    }
}

static bool spark_at(wt_state_t* gs, int r, int c) {
    for (int i = 0; i < gs->spark_count; i++) {
        if ((int)gs->sparks[i].r == r && (int)gs->sparks[i].c == c) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Board generation
// ---------------------------------------------------------------------------

static void generate_board(wt_state_t* gs, int lvl) {
    memset(gs->board, C_WALL, sizeof(gs->board));

    gs->bw = 15 + lvl * 2;
    gs->bh = 11 + lvl;
    if (gs->bw > WT_MAX_W) gs->bw = WT_MAX_W;
    if (gs->bh > WT_MAX_H) gs->bh = WT_MAX_H;
    if (gs->bw % 2 == 0) gs->bw--;
    if (gs->bh % 2 == 0) gs->bh--;

    carve_maze(gs);

    // Open extra passages for multiple paths (Pac-Man loops)
    int open_pct = 20 + lvl * 2;
    if (open_pct > 40) open_pct = 40;
    open_passages(gs, open_pct);

    // Place start (top-left) and goal (bottom-right)
    find_walkable(gs, 1, 1, &gs->start_r, &gs->start_c);
    find_walkable(gs, gs->bh - 2, gs->bw - 2, &gs->goal_r, &gs->goal_c);
    gs->board[gs->start_r][gs->start_c] = C_START;
    gs->board[gs->goal_r][gs->goal_c] = C_GOAL;

    // Ensure path exists
    int attempts = 0;
    while (!path_exists(gs, gs->start_r, gs->start_c, gs->goal_r, gs->goal_c) && attempts < 50) {
        int rr = 1 + (int)(game_rng_next() % (gs->bh - 2));
        int rc = 1 + (int)(game_rng_next() % (gs->bw - 2));
        if (gs->board[rr][rc] == C_WALL) gs->board[rr][rc] = C_FLOOR;
        attempts++;
    }

    // Scatter solder blob collectibles (~30% of floor)
    gs->dots_total = place_dots(gs, 30);
    gs->dots_collected = 0;

    // Via teleporters: 1 pair at low levels, up to 3
    int nvia = (lvl < 3) ? 1 : (lvl < 6) ? 2 : 3;
    place_vias(gs, nvia);

    // Spark enemies: scale with level
    int nspark = (lvl < 2) ? 1 : (lvl < 4) ? 2 : (lvl < 7) ? 3 : 4;
    init_sparks(gs, nspark);

    // Spark speed: frames between moves (lower = faster)
    gs->spark_speed = 6 - lvl;
    if (gs->spark_speed < 2) gs->spark_speed = 2;

    gs->player_r = gs->start_r;
    gs->player_c = gs->start_c;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

#define ROW_BUF_W 120

static void draw_board(wt_state_t* gs, int sr, int sc) {
    int w = gs->bw * 2;
    if (w > ROW_BUF_W) w = ROW_BUF_W;
    bool color = system_config.terminal_ansi_color;

    char rbuf[ROW_BUF_W];
    uint8_t rclr[ROW_BUF_W];

    for (int r = 0; r < gs->bh; r++) {
        int pos = 0;
        for (int c = 0; c < gs->bw && pos + 1 < ROW_BUF_W; c++) {
            // Layer: player on top, then sparks, then board cell
            if (r == gs->player_r && c == gs->player_c) {
                // Blink during invulnerability
                bool blink = (gs->invuln > 0 && (gs->frame_count & 2));
                if (blink) {
                    rbuf[pos] = '.'; rbuf[pos + 1] = ' ';
                    rclr[pos] = CLR_FLOOR; rclr[pos + 1] = CLR_FLOOR;
                } else {
                    rbuf[pos] = 'P'; rbuf[pos + 1] = ' ';
                    rclr[pos] = CLR_PLAYER; rclr[pos + 1] = CLR_PLAYER;
                }
            } else if (spark_at(gs, r, c)) {
                // Alternate * and ~ each frame for sparkly smoke effect
                rbuf[pos] = (gs->frame_count & 1) ? '~' : '*';
                rbuf[pos + 1] = ' ';
                rclr[pos] = CLR_SPARK; rclr[pos + 1] = CLR_SPARK;
            } else {
                uint8_t cell = gs->board[r][c];
                switch (cell) {
                    case C_WALL:
                        // Copper trace: spaces with yellow bg (color) or ## (no color)
                        rbuf[pos]     = color ? ' ' : '#';
                        rbuf[pos + 1] = color ? ' ' : '#';
                        rclr[pos] = CLR_WALL; rclr[pos + 1] = CLR_WALL;
                        break;
                    case C_FLOOR:
                        rbuf[pos] = '.'; rbuf[pos + 1] = ' ';
                        rclr[pos] = CLR_FLOOR; rclr[pos + 1] = CLR_FLOOR;
                        break;
                    case C_DOT:
                        rbuf[pos] = 'o'; rbuf[pos + 1] = ' ';
                        rclr[pos] = CLR_DOT; rclr[pos + 1] = CLR_DOT;
                        break;
                    case C_VIA:
                        rbuf[pos] = '('; rbuf[pos + 1] = ')';
                        rclr[pos] = CLR_VIA; rclr[pos + 1] = CLR_VIA;
                        break;
                    case C_START:
                        rbuf[pos] = 'S'; rbuf[pos + 1] = ' ';
                        rclr[pos] = CLR_START; rclr[pos + 1] = CLR_START;
                        break;
                    case C_GOAL:
                        rbuf[pos] = 'G'; rbuf[pos + 1] = ' ';
                        rclr[pos] = CLR_GOAL; rclr[pos + 1] = CLR_GOAL;
                        break;
                    default:
                        rbuf[pos] = '?'; rbuf[pos + 1] = '?';
                        rclr[pos] = CLR_FLOOR; rclr[pos + 1] = CLR_FLOOR;
                        break;
                }
            }
            pos += 2;
        }

        // Emit row using span-based coloring (printf only, no putchar)
        ui_term_cursor_position(sr + r, sc);
        if (color) {
            uint8_t cur = 255;
            int span_start = 0;
            for (int i = 0; i < pos; i++) {
                if (rclr[i] != cur) {
                    if (i > span_start) {
                        printf("%.*s", i - span_start, &rbuf[span_start]);
                    }
                    cur = rclr[i];
                    printf("%s", clr_lut[cur]);
                    span_start = i;
                }
            }
            if (pos > span_start) {
                printf("%.*s", pos - span_start, &rbuf[span_start]);
            }
            printf("\x1b[0m");
        } else {
            printf("%.*s", pos, rbuf);
        }
        printf("\x1b[K");
    }
}

static void draw_status(wt_state_t* gs, int row, int time_left) {
    ui_term_cursor_position(row, 1);
    printf(" %sWIRE TRACE%s  Lv:%s%d%s  Score:%s%d%s  Hi:%s%d%s  "
           "Dots:%s%d%s/%d  Lives:%s%d%s  Time:%s%02d%s\x1b[K",
           CC("\x1b[1;37m"), RR(),
           CC("\x1b[36m"), gs->level, RR(),
           CC("\x1b[33m"), gs->score, RR(),
           CC("\x1b[31m"), gs->hi_score, RR(),
           CC("\x1b[33m"), gs->dots_collected, RR(), gs->dots_total,
           CC("\x1b[32m"), gs->lives, RR(),
           time_left <= 10 ? CC("\x1b[31m") : CC("\x1b[32m"), time_left, RR());
}

static void draw_legend(int row) {
    ui_term_cursor_position(row, 2);
    if (system_config.terminal_ansi_color) {
        printf("\x1b[0;43m  \x1b[0m=copper  "
               "\x1b[0;37mo\x1b[0m=solder  "
               "\x1b[0;1;36m()\x1b[0m=via  "
               "\x1b[0;2;36m*\x1b[0m=smoke  "
               "\x1b[0;1;32mP\x1b[0m=probe  "
               "\x1b[0;32mS\x1b[0m=start  "
               "\x1b[0;1;37mG\x1b[0m=goal\x1b[K");
    } else {
        printf("##=wall o=dot ()=via *=smoke P=you S=start G=goal\x1b[K");
    }
}

// ---------------------------------------------------------------------------
// Via teleport
// ---------------------------------------------------------------------------

static bool try_via_teleport(wt_state_t* gs, int r, int c) {
    for (int i = 0; i < gs->via_count; i++) {
        if ((int)gs->via_pairs[i].r1 == r && (int)gs->via_pairs[i].c1 == c) {
            gs->player_r = (int)gs->via_pairs[i].r2;
            gs->player_c = (int)gs->via_pairs[i].c2;
            return true;
        }
        if ((int)gs->via_pairs[i].r2 == r && (int)gs->via_pairs[i].c2 == c) {
            gs->player_r = (int)gs->via_pairs[i].r1;
            gs->player_c = (int)gs->via_pairs[i].c1;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------

void wiretrace_handler(struct command_result* res) {
    if (bp_cmd_help_check(&wiretrace_def, res->help_flag)) return;

    wt_state_t gs = {0};

    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 40 || gs.field_h < 18) {
        printf("Terminal too small (need at least 40x18)\r\n");
        return;
    }

    game_rng_seed();
    game_screen_enter(0);

    bool quit = false;

    while (!quit) {
        // === New game ===
        gs.level = 1;
        gs.score = 0;
        gs.lives = 3;
        bool game_over = false;

        while (!game_over && !quit) {
            // === New level ===
            generate_board(&gs, gs.level);

            // Time limit: 60s base, shrinks with level, min 25s
            int time_limit = 60 - (gs.level - 1) * 3;
            if (time_limit < 25) time_limit = 25;

            // Center board on screen
            int board_w = gs.bw * 2;
            int sc = (gs.field_w - board_w) / 2;
            if (sc < 1) sc = 1;
            int sr = 2;
            int status_row = sr + gs.bh + 1;
            int legend_row = sr + gs.bh + 2;

            // Level intro
            printf("\x1b[2J");
            int mid_r = gs.field_h / 2;
            int mid_c = (gs.field_w - 24) / 2;
            ui_term_cursor_position(mid_r - 1, mid_c);
            printf("%s%s=== LEVEL %d ===%s", CC("\x1b[7m"), CC("\x1b[36m"), gs.level, RR());
            ui_term_cursor_position(mid_r, mid_c);
            printf("Board: %dx%d  Time: %ds", gs.bw, gs.bh, time_limit);
            ui_term_cursor_position(mid_r + 1, mid_c);
            printf("Sparks: %d  Vias: %d  Lives: %d", gs.spark_count, gs.via_count, gs.lives);
            tx_fifo_wait_drain();
            busy_wait_ms(1500);
            printf("\x1b[2J");

            gs.frame_count = 0;
            gs.invuln = 20;    // brief invulnerability at level start

            // Draw initial frame
            draw_board(&gs, sr, sc);
            draw_legend(legend_row);
            draw_status(&gs, status_row, time_limit);
            tx_fifo_wait_drain();

            absolute_time_t round_end = make_timeout_time_ms(time_limit * 1000);
            bool level_done = false;
            bool level_won = false;

            while (!level_done && !quit) {
                uint32_t tick_start = time_us_32();

                // Timer
                int64_t rem_us = absolute_time_diff_us(get_absolute_time(), round_end);
                int time_left = (int)(rem_us / 1000000);
                if (time_left < 0) time_left = 0;

                if (rem_us <= 0) {
                    level_done = true;
                    level_won = false;
                    break;
                }

                // Poll all pending input (take the last directional input)
                int move_dr = 0, move_dc = 0;
                char c;
                while (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; break; }
                    if (c == 'w' || c == 'W') { move_dr = -1; move_dc = 0; }
                    if (c == 's' || c == 'S') { move_dr = 1; move_dc = 0; }
                    if (c == 'a' || c == 'A') { move_dc = -1; move_dr = 0; }
                    if (c == 'd' || c == 'D') { move_dc = 1; move_dr = 0; }
                    if (c == 0x1b) {
                        char s1, s2;
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s1) && s1 == '[') {
                            busy_wait_ms(2);
                            if (rx_fifo_try_get(&s2)) {
                                if (s2 == 'A') { move_dr = -1; move_dc = 0; }
                                else if (s2 == 'B') { move_dr = 1; move_dc = 0; }
                                else if (s2 == 'D') { move_dc = -1; move_dr = 0; }
                                else if (s2 == 'C') { move_dc = 1; move_dr = 0; }
                            }
                        }
                    }
                }
                if (quit) break;

                // Move player
                if (move_dr != 0 || move_dc != 0) {
                    int nr = gs.player_r + move_dr;
                    int nc = gs.player_c + move_dc;
                    if (in_bounds(&gs, nr, nc) && gs.board[nr][nc] != C_WALL) {
                        gs.player_r = nr;
                        gs.player_c = nc;

                        // Collect solder blob
                        if (gs.board[nr][nc] == C_DOT) {
                            gs.board[nr][nc] = C_FLOOR;
                            gs.dots_collected++;
                            gs.score += 10;
                        }

                        // Via teleport
                        if (gs.board[nr][nc] == C_VIA) {
                            if (try_via_teleport(&gs, nr, nc)) {
                                gs.invuln = 3; // brief grace after teleport
                            }
                        }

                        // Check goal (also after possible teleport)
                        if (gs.player_r == gs.goal_r && gs.player_c == gs.goal_c) {
                            level_done = true;
                            level_won = true;
                        }
                    }
                }

                // Move sparks on their tick schedule
                gs.frame_count++;
                if (gs.frame_count % gs.spark_speed == 0) {
                    move_sparks(&gs);
                }

                // Spark collision check
                if (gs.invuln > 0) {
                    gs.invuln--;
                } else if (spark_at(&gs, gs.player_r, gs.player_c)) {
                    gs.lives--;
                    if (gs.lives <= 0) {
                        level_done = true;
                        level_won = false;
                    } else {
                        // Respawn at start, brief invulnerability
                        gs.player_r = gs.start_r;
                        gs.player_c = gs.start_c;
                        gs.invuln = 20;
                    }
                }

                // Draw
                draw_board(&gs, sr, sc);
                draw_status(&gs, status_row, time_left);
                tx_fifo_wait_drain();

                // Frame timing
                uint32_t elapsed = (time_us_32() - tick_start) / 1000;
                if ((int)elapsed < TICK_MS) {
                    busy_wait_ms(TICK_MS - (int)elapsed);
                }
            }

            if (quit) break;

            // === Level result ===
            int64_t end_rem = absolute_time_diff_us(get_absolute_time(), round_end);
            int time_left = (int)(end_rem / 1000000);
            if (time_left < 0) time_left = 0;

            int mid = gs.field_h / 2;
            int mc = (gs.field_w - 30) / 2;
            if (mc < 1) mc = 1;

            printf("\x1b[2J");

            if (level_won) {
                int bonus = time_left * gs.level;
                gs.score += bonus;

                ui_term_cursor_position(mid - 2, mc);
                printf("%s%s+----------------------------+%s", CC("\x1b[7m"), CC("\x1b[32m"), RR());
                ui_term_cursor_position(mid - 1, mc);
                printf("%s%s|     TRACE COMPLETE!        |%s", CC("\x1b[7m"), CC("\x1b[32m"), RR());
                ui_term_cursor_position(mid, mc);
                printf("%s%s+----------------------------+%s", CC("\x1b[7m"), CC("\x1b[32m"), RR());
                ui_term_cursor_position(mid + 2, mc);
                printf("  Dots:  %d/%d  (%s+%d pts%s)",
                       gs.dots_collected, gs.dots_total,
                       CC("\x1b[33m"), gs.dots_collected * 10, RR());
                ui_term_cursor_position(mid + 3, mc);
                printf("  Time:  %d x Lv%d = %s+%d pts%s",
                       time_left, gs.level, CC("\x1b[33m"), bonus, RR());
                ui_term_cursor_position(mid + 4, mc);
                printf("  Total: %s%d%s", CC("\x1b[33m"), gs.score, RR());
                ui_term_cursor_position(mid + 6, mc);
                printf("  SPACE = next level   q = quit");
                tx_fifo_wait_drain();

                bool waiting = true;
                while (waiting) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; waiting = false; }
                        if (ch == ' ') { gs.level++; waiting = false; }
                    }
                    busy_wait_ms(50);
                }
            } else {
                // Game over: lost all lives or timed out
                game_over = true;
                if (gs.score > gs.hi_score) gs.hi_score = gs.score;

                ui_term_cursor_position(mid - 2, mc);
                printf("%s%s+----------------------------+%s", CC("\x1b[7m"), CC("\x1b[31m"), RR());
                ui_term_cursor_position(mid - 1, mc);
                if (gs.lives <= 0) {
                    printf("%s%s|  THE MAGIC SMOKE ESCAPES!  |%s", CC("\x1b[7m"), CC("\x1b[31m"), RR());
                } else {
                    printf("%s%s|       TIME'S UP!           |%s", CC("\x1b[7m"), CC("\x1b[31m"), RR());
                }
                ui_term_cursor_position(mid, mc);
                printf("%s%s+----------------------------+%s", CC("\x1b[7m"), CC("\x1b[31m"), RR());
                ui_term_cursor_position(mid + 2, mc);
                printf("  Level:       %s%d%s", CC("\x1b[36m"), gs.level, RR());
                ui_term_cursor_position(mid + 3, mc);
                printf("  Final Score: %s%d%s", CC("\x1b[33m"), gs.score, RR());
                ui_term_cursor_position(mid + 4, mc);
                printf("  Hi-Score:    %s%d%s", CC("\x1b[31m"), gs.hi_score, RR());
                ui_term_cursor_position(mid + 6, mc);
                printf("  SPACE = new game   q = quit");
                tx_fifo_wait_drain();

                bool waiting = true;
                while (waiting) {
                    char ch;
                    if (rx_fifo_try_get(&ch)) {
                        if (ch == 'q' || ch == 'Q') { quit = true; waiting = false; }
                        if (ch == ' ') { waiting = false; }
                    }
                    busy_wait_ms(50);
                }
            }
        }
    }

    if (gs.score > gs.hi_score) gs.hi_score = gs.score;
    game_screen_exit();
}
