// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file rogueprobe.c
 * @brief Rogue Probe — tiny roguelike for Bus Pirate.
 *
 * You are @, a logic probe exploring a procedurally generated chip die.
 * Rooms are IC functional blocks (ALU, REG, ROM, RAM, CLK, I/O…).
 * Corridors are PCB traces connecting them.
 *
 * Find the firmware dump ($) and escape through the pad (>).
 * Avoid ESD sparks (z), watchdog timers (W), and brownout detectors (~).
 * Collect decoupling caps, bypass jumpers, and test points to survive.
 *
 * Turn-based: enemies move after each player step. No timing pressure.
 * Limited field of view — your probe tip illuminates nearby cells.
 *
 * Controls: WASD / arrows = move, SPACE = wait a turn, q = quit.
 *
 * RAM note: map cells are bitpacked (tile|vis|item in one byte).
 * 40×20 = 800 bytes total map storage.
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
#include "rogueprobe.h"

static const char* const usage[] = {
    "rogue",
    "Launch Rogue Probe:%s rogue",
    "",
    "Roguelike: explore a chip die, find the firmware dump!",
    "WASD/arrows=move  SPACE=wait  q=quit",
};

const bp_command_def_t rogueprobe_def = {
    .name = "rogue",
    .description = T_HELP_ROGUEPROBE,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MAP_W       40
#define MAP_H       20
#define MAX_ROOMS   8
#define MIN_ROOMS   4
#define MAX_ENEMIES 10
#define MAX_ITEMS   10
#define FOV_RADIUS  5
#define MAX_FLOOR   5
#define MSG_MAX     56

// Tile types (3 bits: 0-6)
#define T_VOID      0
#define T_FLOOR     1
#define T_WALL      2
#define T_CORR      3
#define T_DOOR      4
#define T_STAIRS    5
#define T_FWDUMP    6

// Item types (3 bits: 0-4)
#define ITEM_NONE   0
#define ITEM_CAP    1       // decoupling cap — +1 shield
#define ITEM_TPOINT 2       // test point — reveal whole floor
#define ITEM_JUMPER 3       // bypass jumper — +3 HP
#define ITEM_PROBE  4       // scope probe — +2 FOV

// Visibility (2 bits: 0-2)
#define VIS_UNSEEN  0
#define VIS_SEEN    1       // previously seen (dim)
#define VIS_LIT     2       // currently visible (bright)

// Enemy types
#define E_ESD       0       // z — random walk
#define E_WDOG      1       // W — chases player
#define E_BROWN     2       // ~ — patrols corridors

// ---------------------------------------------------------------------------
// Bitpacked cell: [7:5]=item  [4:3]=vis  [2:0]=tile
// ---------------------------------------------------------------------------
#define CELL_TILE(c)      ((c) & 0x07)
#define CELL_VIS(c)       (((c) >> 3) & 0x03)
#define CELL_ITEM(c)      (((c) >> 5) & 0x07)
#define SET_TILE(c, t)    ((c) = ((c) & ~0x07) | ((t) & 0x07))
#define SET_VIS(c, v)     ((c) = ((c) & ~0x18) | (((v) & 0x03) << 3))
#define SET_ITEM(c, i)    ((c) = ((c) & ~0xE0) | (((i) & 0x07) << 5))

// ---------------------------------------------------------------------------
// ANSI helpers
// ---------------------------------------------------------------------------
static const char* CC(const char* c) { return system_config.terminal_ansi_color ? c : ""; }
static const char* RR(void) { return system_config.terminal_ansi_color ? "\x1b[0m" : ""; }

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------
typedef struct { uint8_t x, y, w, h; char label[4]; } room_t;
typedef struct { uint8_t x, y, type, hp; int8_t dx, dy; } enemy_t;

typedef struct {
    uint8_t cells[MAP_H][MAP_W];
    room_t rooms[MAX_ROOMS];
    int room_count;
    enemy_t enemies[MAX_ENEMIES];
    int enemy_count;
    int px, py;
    int hp, max_hp;
    int shield;
    int fov_r;
    int floor_num;
    bool has_fwdump;
    int kills, turns;
    char msg[MSG_MAX + 1];
    int vp_x, vp_y, vp_w, vp_h;
    int field_w, field_h;
} rogue_state_t;

// IC block labels
static const char* const block_names[] = {
    "ALU", "REG", "ROM", "RAM", "CLK", "I/O",
    "ADC", "DAC", "PLL", "DMA", "SPI", "USB",
    "PWM", "WDT", "FLS", "BUS", "IRQ", "TMR",
};
#define N_BLOCKS (sizeof(block_names) / sizeof(block_names[0]))

// ---------------------------------------------------------------------------
// Map helpers
// ---------------------------------------------------------------------------
static uint8_t get_tile(rogue_state_t* gs, int x, int y) { return CELL_TILE(gs->cells[y][x]); }
static uint8_t get_vis(rogue_state_t* gs, int x, int y)  { return CELL_VIS(gs->cells[y][x]); }
static uint8_t get_item(rogue_state_t* gs, int x, int y) { return CELL_ITEM(gs->cells[y][x]); }

static void set_tile(rogue_state_t* gs, int x, int y, uint8_t t) { SET_TILE(gs->cells[y][x], t); }
static void set_vis(rogue_state_t* gs, int x, int y, uint8_t v)  { SET_VIS(gs->cells[y][x], v); }
static void set_item(rogue_state_t* gs, int x, int y, uint8_t i) { SET_ITEM(gs->cells[y][x], i); }

static bool walkable(rogue_state_t* gs, int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return false;
    uint8_t t = get_tile(gs, x, y);
    return t == T_FLOOR || t == T_CORR || t == T_DOOR || t == T_STAIRS || t == T_FWDUMP;
}

// ---------------------------------------------------------------------------
// Map generation
// ---------------------------------------------------------------------------
static bool rects_overlap(int x1, int y1, int w1, int h1,
                          int x2, int y2, int w2, int h2) {
    return !(x1 + w1 + 1 < x2 || x2 + w2 + 1 < x1 ||
             y1 + h1 + 1 < y2 || y2 + h2 + 1 < y1);
}

static void carve_room(rogue_state_t* gs, room_t* r) {
    for (int y = r->y - 1; y <= r->y + r->h; y++) {
        for (int x = r->x - 1; x <= r->x + r->w; x++) {
            if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) {
                if (y == r->y - 1 || y == r->y + r->h ||
                    x == r->x - 1 || x == r->x + r->w) {
                    if (get_tile(gs, x, y) == T_VOID) set_tile(gs, x, y, T_WALL);
                } else {
                    set_tile(gs, x, y, T_FLOOR);
                }
            }
        }
    }
}

static void carve_corridor(rogue_state_t* gs, int x1, int y1, int x2, int y2) {
    int cx = x1, cy = y1;
    bool h_first = (game_rng_next() & 1);

    for (int pass = 0; pass < 2; pass++) {
        bool do_h = (pass == 0) ? h_first : !h_first;
        if (do_h) {
            int dx = (x2 > cx) ? 1 : -1;
            while (cx != x2) {
                cx += dx;
                if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) {
                    uint8_t t = get_tile(gs, cx, cy);
                    if (t == T_VOID) set_tile(gs, cx, cy, T_CORR);
                    else if (t == T_WALL) set_tile(gs, cx, cy, T_DOOR);
                }
            }
        } else {
            int dy = (y2 > cy) ? 1 : -1;
            while (cy != y2) {
                cy += dy;
                if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) {
                    uint8_t t = get_tile(gs, cx, cy);
                    if (t == T_VOID) set_tile(gs, cx, cy, T_CORR);
                    else if (t == T_WALL) set_tile(gs, cx, cy, T_DOOR);
                }
            }
        }
    }
}

static void generate_map(rogue_state_t* gs) {
    memset(gs->cells, 0, sizeof(gs->cells));  // all T_VOID, VIS_UNSEEN, ITEM_NONE
    gs->room_count = 0;
    gs->enemy_count = 0;

    int target = MIN_ROOMS + (int)(game_rng_next() % (MAX_ROOMS - MIN_ROOMS + 1));
    int attempts = 0;

    while (gs->room_count < target && attempts < 200) {
        attempts++;
        int rw = 4 + (int)(game_rng_next() % 5);   // 4–8
        int rh = 3 + (int)(game_rng_next() % 4);    // 3–6
        int rx = 1 + (int)(game_rng_next() % (MAP_W - rw - 2));
        int ry = 1 + (int)(game_rng_next() % (MAP_H - rh - 2));

        bool ok = true;
        for (int i = 0; i < gs->room_count; i++) {
            if (rects_overlap(rx, ry, rw, rh,
                              gs->rooms[i].x, gs->rooms[i].y, gs->rooms[i].w, gs->rooms[i].h)) {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        room_t* r = &gs->rooms[gs->room_count];
        r->x = (uint8_t)rx;
        r->y = (uint8_t)ry;
        r->w = (uint8_t)rw;
        r->h = (uint8_t)rh;
        int li = (int)(game_rng_next() % N_BLOCKS);
        memcpy(r->label, block_names[li], 3);
        r->label[3] = '\0';

        carve_room(gs, r);
        gs->room_count++;
    }

    // Connect rooms with corridors
    for (int i = 1; i < gs->room_count; i++) {
        int cx1 = gs->rooms[i - 1].x + gs->rooms[i - 1].w / 2;
        int cy1 = gs->rooms[i - 1].y + gs->rooms[i - 1].h / 2;
        int cx2 = gs->rooms[i].x + gs->rooms[i].w / 2;
        int cy2 = gs->rooms[i].y + gs->rooms[i].h / 2;
        carve_corridor(gs, cx1, cy1, cx2, cy2);
    }
    // Extra corridors for loops
    for (int i = 0; i < 2 && gs->room_count > 2; i++) {
        int a = (int)(game_rng_next() % gs->room_count);
        int b = (int)(game_rng_next() % gs->room_count);
        if (a == b) continue;
        carve_corridor(gs, gs->rooms[a].x + gs->rooms[a].w / 2, gs->rooms[a].y + gs->rooms[a].h / 2,
                       gs->rooms[b].x + gs->rooms[b].w / 2, gs->rooms[b].y + gs->rooms[b].h / 2);
    }
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------
static void place_player(rogue_state_t* gs) {
    room_t* r = &gs->rooms[0];
    gs->px = r->x + (int)(game_rng_next() % r->w);
    gs->py = r->y + (int)(game_rng_next() % r->h);
}

static void place_stairs(rogue_state_t* gs) {
    room_t* r = &gs->rooms[gs->room_count - 1];
    int sx = r->x + (int)(game_rng_next() % r->w);
    int sy = r->y + (int)(game_rng_next() % r->h);
    set_tile(gs, sx, sy, T_STAIRS);
}

static void place_fwdump(rogue_state_t* gs) {
    int ri = (gs->room_count >= 3) ? 1 + (int)(game_rng_next() % (gs->room_count - 2))
                                   : gs->room_count - 1;
    room_t* r = &gs->rooms[ri];
    int fx = r->x + (int)(game_rng_next() % r->w);
    int fy = r->y + (int)(game_rng_next() % r->h);
    set_tile(gs, fx, fy, T_FWDUMP);
}

// Scatter items (stored in the cell bitfield)
static void scatter_items(rogue_state_t* gs) {
    int count = 3 + gs->floor_num;
    if (count > MAX_ITEMS) count = MAX_ITEMS;

    for (int i = 0; i < count; i++) {
        int ri = (gs->room_count > 1) ? 1 + (int)(game_rng_next() % (gs->room_count - 1)) : 0;
        room_t* r = &gs->rooms[ri];
        int ix = r->x + (int)(game_rng_next() % r->w);
        int iy = r->y + (int)(game_rng_next() % r->h);
        if (get_tile(gs, ix, iy) != T_FLOOR || get_item(gs, ix, iy) != ITEM_NONE) continue;

        int roll = (int)(game_rng_next() % 100);
        uint8_t it;
        if (roll < 40)       it = ITEM_CAP;
        else if (roll < 65)  it = ITEM_JUMPER;
        else if (roll < 85)  it = ITEM_TPOINT;
        else                 it = ITEM_PROBE;
        set_item(gs, ix, iy, it);
    }
}

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------
static void spawn_enemies(rogue_state_t* gs) {
    gs->enemy_count = 0;
    int count = 2 + gs->floor_num * 2;
    if (count > MAX_ENEMIES) count = MAX_ENEMIES;

    for (int i = 0; i < count; i++) {
        int ri = (gs->room_count > 1) ? 1 + (int)(game_rng_next() % (gs->room_count - 1)) : 0;
        room_t* r = &gs->rooms[ri];
        int ex = r->x + (int)(game_rng_next() % r->w);
        int ey = r->y + (int)(game_rng_next() % r->h);
        if (!walkable(gs, ex, ey)) continue;

        enemy_t* e = &gs->enemies[gs->enemy_count];
        e->x = (uint8_t)ex;
        e->y = (uint8_t)ey;
        int roll = (int)(game_rng_next() % 100);
        if (roll < 50) {
            e->type = E_ESD;
            e->hp = (uint8_t)(1 + gs->floor_num / 2);
        } else if (roll < 80) {
            e->type = E_WDOG;
            e->hp = (uint8_t)(2 + gs->floor_num / 2);
        } else {
            e->type = E_BROWN;
            e->hp = (uint8_t)(1 + gs->floor_num / 3);
        }
        e->dx = 0;
        e->dy = 0;
        gs->enemy_count++;
    }
}

static int enemy_at(rogue_state_t* gs, int x, int y) {
    for (int i = 0; i < gs->enemy_count; i++) {
        if ((int)gs->enemies[i].x == x && (int)gs->enemies[i].y == y) return i;
    }
    return -1;
}

static void remove_enemy(rogue_state_t* gs, int idx) {
    if (idx < 0 || idx >= gs->enemy_count) return;
    gs->enemies[idx] = gs->enemies[gs->enemy_count - 1];
    gs->enemy_count--;
}

static void move_enemies(rogue_state_t* gs) {
    for (int i = 0; i < gs->enemy_count; i++) {
        enemy_t* e = &gs->enemies[i];
        int nx, ny;

        switch (e->type) {
            case E_ESD: {
                static const int edx[] = {0, 0, -1, 1};
                static const int edy[] = {-1, 1, 0, 0};
                int d = (int)(game_rng_next() % 4);
                nx = (int)e->x + edx[d];
                ny = (int)e->y + edy[d];
                if (walkable(gs, nx, ny)) { e->x = (uint8_t)nx; e->y = (uint8_t)ny; }
                break;
            }
            case E_WDOG: {
                int ddx = 0, ddy = 0;
                if (abs(gs->px - (int)e->x) >= abs(gs->py - (int)e->y)) {
                    ddx = (gs->px > (int)e->x) ? 1 : (gs->px < (int)e->x) ? -1 : 0;
                } else {
                    ddy = (gs->py > (int)e->y) ? 1 : (gs->py < (int)e->y) ? -1 : 0;
                }
                nx = (int)e->x + ddx;
                ny = (int)e->y + ddy;
                if (walkable(gs, nx, ny)) {
                    e->x = (uint8_t)nx;
                    e->y = (uint8_t)ny;
                } else {
                    if (ddx != 0) {
                        ddy = (gs->py > (int)e->y) ? 1 : (gs->py < (int)e->y) ? -1 : 0;
                        ddx = 0;
                    } else {
                        ddx = (gs->px > (int)e->x) ? 1 : (gs->px < (int)e->x) ? -1 : 0;
                        ddy = 0;
                    }
                    nx = (int)e->x + ddx;
                    ny = (int)e->y + ddy;
                    if (walkable(gs, nx, ny)) { e->x = (uint8_t)nx; e->y = (uint8_t)ny; }
                }
                break;
            }
            case E_BROWN: {
                if (e->dx == 0 && e->dy == 0) {
                    static const int bdx[] = {0, 0, -1, 1};
                    static const int bdy[] = {-1, 1, 0, 0};
                    int d = (int)(game_rng_next() % 4);
                    e->dx = (int8_t)bdx[d];
                    e->dy = (int8_t)bdy[d];
                }
                nx = (int)e->x + e->dx;
                ny = (int)e->y + e->dy;
                if (walkable(gs, nx, ny)) {
                    e->x = (uint8_t)nx;
                    e->y = (uint8_t)ny;
                } else {
                    e->dx = -e->dx;
                    e->dy = -e->dy;
                    nx = (int)e->x + e->dx;
                    ny = (int)e->y + e->dy;
                    if (walkable(gs, nx, ny)) { e->x = (uint8_t)nx; e->y = (uint8_t)ny; }
                }
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// FOV
// ---------------------------------------------------------------------------
static bool los_clear(rogue_state_t* gs, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int cx = x0, cy = y0;

    while (cx != x1 || cy != y1) {
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx)  { err += dx; cy += sy; }
        if (cx == x1 && cy == y1) break;
        if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) return false;
        uint8_t t = get_tile(gs, cx, cy);
        if (t == T_WALL || t == T_VOID) return false;
    }
    return true;
}

static void compute_fov(rogue_state_t* gs) {
    // Dim all currently-lit cells to "seen"
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (get_vis(gs, x, y) == VIS_LIT) set_vis(gs, x, y, VIS_SEEN);

    // Light cells within radius with LOS
    for (int dy = -gs->fov_r; dy <= gs->fov_r; dy++) {
        for (int dx = -gs->fov_r; dx <= gs->fov_r; dx++) {
            if (dx * dx + dy * dy > gs->fov_r * gs->fov_r) continue;
            int tx = gs->px + dx, ty = gs->py + dy;
            if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) continue;
            if (get_tile(gs, tx, ty) == T_VOID) continue;
            if (los_clear(gs, gs->px, gs->py, tx, ty))
                set_vis(gs, tx, ty, VIS_LIT);
        }
    }
    set_vis(gs, gs->px, gs->py, VIS_LIT);
}

static void reveal_all(rogue_state_t* gs) {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (get_tile(gs, x, y) != T_VOID) set_vis(gs, x, y, VIS_LIT);
}

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------
static void update_viewport(rogue_state_t* gs) {
    gs->vp_x = gs->px - gs->vp_w / 2;
    gs->vp_y = gs->py - gs->vp_h / 2;
    if (gs->vp_x < 0) gs->vp_x = 0;
    if (gs->vp_y < 0) gs->vp_y = 0;
    if (gs->vp_x + gs->vp_w > MAP_W) gs->vp_x = MAP_W - gs->vp_w;
    if (gs->vp_y + gs->vp_h > MAP_H) gs->vp_y = MAP_H - gs->vp_h;
    if (gs->vp_x < 0) gs->vp_x = 0;
    if (gs->vp_y < 0) gs->vp_y = 0;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static char tile_char(uint8_t tile) {
    switch (tile) {
        case T_FLOOR:  return '.';
        case T_WALL:   return '#';
        case T_CORR:   return '.';
        case T_DOOR:   return '+';
        case T_STAIRS: return '>';
        case T_FWDUMP: return '$';
        default:       return ' ';
    }
}

static char item_char(uint8_t it) {
    switch (it) {
        case ITEM_CAP:    return 'C';
        case ITEM_TPOINT: return 'T';
        case ITEM_JUMPER: return 'J';
        case ITEM_PROBE:  return 'S';
        default:          return '\0';
    }
}

static const char* tile_color_lit(uint8_t tile) {
    switch (tile) {
        case T_FLOOR:  return "\x1b[0;37m";
        case T_WALL:   return "\x1b[0;1;33m";
        case T_CORR:   return "\x1b[0;36m";
        case T_DOOR:   return "\x1b[0;1;33m";
        case T_STAIRS: return "\x1b[0;1;37m";
        case T_FWDUMP: return "\x1b[0;1;32m";
        default:       return "\x1b[0;2;30m";
    }
}

static const char* tile_color_dim(uint8_t tile) {
    switch (tile) {
        case T_FLOOR:  return "\x1b[0;2;37m";
        case T_WALL:   return "\x1b[0;2;33m";
        case T_CORR:   return "\x1b[0;2;36m";
        case T_DOOR:   return "\x1b[0;2;33m";
        case T_STAIRS: return "\x1b[0;2;37m";
        case T_FWDUMP: return "\x1b[0;2;32m";
        default:       return "\x1b[0;2;30m";
    }
}

static void draw_map(rogue_state_t* gs, int sr, int sc) {
    bool color = system_config.terminal_ansi_color;

    for (int vy = 0; vy < gs->vp_h; vy++) {
        int my = gs->vp_y + vy;
        ui_term_cursor_position(sr + vy, sc);

        for (int vx = 0; vx < gs->vp_w; vx++) {
            int mx = gs->vp_x + vx;

            if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H ||
                get_vis(gs, mx, my) == VIS_UNSEEN) {
                printf("%s ", color ? "\x1b[0m" : "");
                continue;
            }

            bool lit = (get_vis(gs, mx, my) == VIS_LIT);

            // Player
            if (mx == gs->px && my == gs->py) {
                printf("%s@", color ? "\x1b[0;1;32m" : "");
                continue;
            }

            // Enemy (only if lit)
            if (lit) {
                int ei = enemy_at(gs, mx, my);
                if (ei >= 0) {
                    const char* ec;
                    char ech;
                    switch (gs->enemies[ei].type) {
                        case E_ESD:   ec = "\x1b[0;1;34m"; ech = 'z'; break;
                        case E_WDOG:  ec = "\x1b[0;1;31m"; ech = 'W'; break;
                        case E_BROWN: ec = "\x1b[0;33m";    ech = '~'; break;
                        default:      ec = "\x1b[0;31m";    ech = '?'; break;
                    }
                    printf("%s%c", color ? ec : "", ech);
                    continue;
                }
            }

            // Item (only if lit)
            if (lit) {
                uint8_t it = get_item(gs, mx, my);
                if (it != ITEM_NONE) {
                    char ic = item_char(it);
                    if (ic) {
                        printf("%s%c", color ? "\x1b[0;1;35m" : "", ic);
                        continue;
                    }
                }
            }

            // Tile
            uint8_t tile = get_tile(gs, mx, my);
            char ch = tile_char(tile);
            if (color) {
                printf("%s%c", lit ? tile_color_lit(tile) : tile_color_dim(tile), ch);
            } else {
                printf("%c", lit ? ch : (ch == '#' ? '#' : ' '));
            }
        }
        printf("%s\x1b[K", RR());
    }
}

static void draw_hud(rogue_state_t* gs, int row) {
    ui_term_cursor_position(row, 1);

    const char* fw_str = gs->has_fwdump ? "YES" : "no";
    const char* fw_clr = gs->has_fwdump ? "\x1b[0;1;32m" : "\x1b[0;2;31m";

    printf(" %sROGUE PROBE%s  F%s%d%s  HP:%s%d%s/%d  Sh:%s%d%s  "
           "FOV:%s%d%s  FW:%s%s%s  K:%s%d%s  T:%d\x1b[K",
           CC("\x1b[0;1;37m"), RR(),
           CC("\x1b[0;36m"), gs->floor_num, RR(),
           gs->hp <= 3 ? CC("\x1b[0;1;31m") : CC("\x1b[0;1;32m"), gs->hp, RR(), gs->max_hp,
           CC("\x1b[0;1;36m"), gs->shield, RR(),
           CC("\x1b[0;33m"), gs->fov_r, RR(),
           CC(fw_clr), fw_str, RR(),
           CC("\x1b[0;31m"), gs->kills, RR(),
           gs->turns);
}

static void draw_msg(rogue_state_t* gs, int row) {
    ui_term_cursor_position(row, 1);
    if (gs->msg[0]) {
        printf(" %s%s%s\x1b[K", CC("\x1b[0;1;33m"), gs->msg, RR());
    } else {
        printf("\x1b[K");
    }
}

static void draw_legend(int row) {
    ui_term_cursor_position(row, 1);
    if (system_config.terminal_ansi_color) {
        printf(" %s@%s=you %s#%s=IC %s.%s=die %s+%s=door "
               "%s>%s=exit %s$%s=fw "
               "%sz%s=ESD %sW%s=wdog %s~%s=brn\x1b[K",
               "\x1b[0;1;32m", RR(), "\x1b[0;1;33m", RR(),
               "\x1b[0;37m", RR(), "\x1b[0;1;33m", RR(),
               "\x1b[0;1;37m", RR(), "\x1b[0;1;32m", RR(),
               "\x1b[0;1;34m", RR(), "\x1b[0;1;31m", RR(),
               "\x1b[0;33m", RR());
    } else {
        printf(" @=you #=IC .=die +=door >=exit $=fw z=ESD W=wdog ~=brn\x1b[K");
    }
}

static void draw_room_label(rogue_state_t* gs, int sr, int sc) {
    for (int i = 0; i < gs->room_count; i++) {
        room_t* r = &gs->rooms[i];
        if (gs->px >= (int)r->x && gs->px < (int)r->x + (int)r->w &&
            gs->py >= (int)r->y && gs->py < (int)r->y + (int)r->h) {
            int lx = (int)r->x - gs->vp_x + sc;
            int ly = (int)r->y - 1 - gs->vp_y + sr;
            if (ly >= sr && ly < sr + gs->vp_h && lx >= sc && lx + 3 < sc + gs->vp_w) {
                ui_term_cursor_position(ly, lx);
                printf("%s%s%s", CC("\x1b[0;7;33m"), r->label, RR());
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------
static void combat_bump(rogue_state_t* gs, int ex, int ey) {
    int ei = enemy_at(gs, ex, ey);
    if (ei < 0) return;

    int dmg = 1 + (int)(game_rng_next() % 2);
    gs->enemies[ei].hp -= (uint8_t)dmg;

    if (gs->enemies[ei].hp <= 0) {
        const char* names[] = { "ESD spark", "Watchdog", "Brownout" };
        int et = gs->enemies[ei].type;
        if (et > 2) et = 0;
        snprintf(gs->msg, MSG_MAX, "Destroyed %s! (+%d pts)", names[et], (et + 1) * 10);
        gs->kills++;
        remove_enemy(gs, ei);
    } else {
        snprintf(gs->msg, MSG_MAX, "Hit! Enemy HP: %d", gs->enemies[ei].hp);
    }
}

static void enemy_attacks(rogue_state_t* gs) {
    for (int i = 0; i < gs->enemy_count; i++) {
        int dx = abs((int)gs->enemies[i].x - gs->px);
        int dy = abs((int)gs->enemies[i].y - gs->py);
        if (dx <= 1 && dy <= 1 && (dx + dy) <= 1) {
            int dmg = 1;
            if (gs->enemies[i].type == E_WDOG) dmg = 2;

            if (gs->shield > 0) {
                gs->shield--;
                snprintf(gs->msg, MSG_MAX, "Shield absorbed hit! (%d left)", gs->shield);
            } else {
                gs->hp -= dmg;
                const char* names[] = { "ESD zaps", "Watchdog bites", "Brownout drains" };
                int et = gs->enemies[i].type;
                if (et > 2) et = 0;
                snprintf(gs->msg, MSG_MAX, "%s you! -%d HP", names[et], dmg);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Item pickup
// ---------------------------------------------------------------------------
static void check_pickup(rogue_state_t* gs) {
    uint8_t it = get_item(gs, gs->px, gs->py);
    if (it == ITEM_NONE) return;

    switch (it) {
        case ITEM_CAP:
            gs->shield++;
            snprintf(gs->msg, MSG_MAX, "Decoupling Cap! Shield +1 (%d)", gs->shield);
            break;
        case ITEM_TPOINT:
            reveal_all(gs);
            snprintf(gs->msg, MSG_MAX, "Test Point! Floor revealed!");
            break;
        case ITEM_JUMPER:
            gs->hp += 3;
            if (gs->hp > gs->max_hp + 2) gs->hp = gs->max_hp + 2;
            snprintf(gs->msg, MSG_MAX, "Bypass Jumper! HP +3 (%d)", gs->hp);
            break;
        case ITEM_PROBE:
            gs->fov_r += 2;
            snprintf(gs->msg, MSG_MAX, "Scope Probe! FOV +2 (%d)", gs->fov_r);
            break;
    }
    set_item(gs, gs->px, gs->py, ITEM_NONE);
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void rogueprobe_handler(struct command_result* res) {
    if (bp_cmd_help_check(&rogueprobe_def, res->help_flag)) return;

    rogue_state_t gs = {0};
    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 40 || gs.field_h < 18) {
        printf("Terminal too small (need 40x18+)\r\n");
        return;
    }

    game_rng_seed();
    game_screen_enter(0);

    bool quit = false;

    while (!quit) {
        // === New game ===
        gs.floor_num = 1;
        gs.hp = 10;
        gs.max_hp = 10;
        gs.shield = 0;
        gs.fov_r = FOV_RADIUS;
        gs.has_fwdump = false;
        gs.kills = 0;
        gs.turns = 0;
        gs.msg[0] = '\0';
        bool game_over = false;

        while (!game_over && !quit) {
            // === New floor ===
            generate_map(&gs);
            place_player(&gs);
            if (!gs.has_fwdump && gs.floor_num >= 3) place_fwdump(&gs);
            place_stairs(&gs);
            scatter_items(&gs);
            spawn_enemies(&gs);

            snprintf(gs.msg, MSG_MAX, "Floor %d: %s block...",
                     gs.floor_num, gs.rooms[gs.room_count > 1 ? 1 : 0].label);

            gs.vp_w = gs.field_w - 2;
            if (gs.vp_w > MAP_W) gs.vp_w = MAP_W;
            gs.vp_h = gs.field_h - 5;
            if (gs.vp_h > MAP_H) gs.vp_h = MAP_H;

            int sc = (gs.field_w - gs.vp_w) / 2 + 1;
            int sr = 2;
            int hud_row = sr + gs.vp_h + 1;
            int msg_row = sr + gs.vp_h + 2;
            int leg_row = sr + gs.vp_h + 3;

            printf("\x1b[2J");

            compute_fov(&gs);
            update_viewport(&gs);
            draw_map(&gs, sr, sc);
            draw_room_label(&gs, sr, sc);
            draw_hud(&gs, hud_row);
            draw_msg(&gs, msg_row);
            draw_legend(leg_row);
            tx_fifo_wait_drain();

            bool floor_done = false;

            while (!floor_done && !game_over && !quit) {
                game_input_t inp;
                bool got = false;
                while (!got) {
                    got = game_poll_input(&inp, GAME_INPUT_WASD);
                    if (!got) busy_wait_ms(30);
                }

                if (inp.type == GAME_INPUT_QUIT) { quit = true; break; }

                gs.msg[0] = '\0';

                int dx = 0, dy = 0;
                switch (inp.type) {
                    case GAME_INPUT_UP:    dy = -1; break;
                    case GAME_INPUT_DOWN:  dy = 1; break;
                    case GAME_INPUT_LEFT:  dx = -1; break;
                    case GAME_INPUT_RIGHT: dx = 1; break;
                    case GAME_INPUT_ACTION:
                        snprintf(gs.msg, MSG_MAX, "Waiting...");
                        break;
                    default:
                        continue;
                }

                int nx = gs.px + dx, ny = gs.py + dy;

                if (dx != 0 || dy != 0) {
                    int ei = enemy_at(&gs, nx, ny);
                    if (ei >= 0) {
                        combat_bump(&gs, nx, ny);
                    } else if (walkable(&gs, nx, ny)) {
                        gs.px = nx;
                        gs.py = ny;

                        if (get_tile(&gs, gs.px, gs.py) == T_FWDUMP) {
                            gs.has_fwdump = true;
                            set_tile(&gs, gs.px, gs.py, T_FLOOR);
                            snprintf(gs.msg, MSG_MAX, "*** FIRMWARE DUMP ACQUIRED! ***");
                        } else if (get_tile(&gs, gs.px, gs.py) == T_STAIRS) {
                            if (gs.has_fwdump) {
                                floor_done = true;
                                game_over = true;
                            } else if (gs.floor_num < MAX_FLOOR) {
                                floor_done = true;
                                snprintf(gs.msg, MSG_MAX, "Descending...");
                            } else {
                                snprintf(gs.msg, MSG_MAX, "Locked! Find firmware first!");
                            }
                        }
                        check_pickup(&gs);
                    }
                }

                gs.turns++;
                move_enemies(&gs);
                enemy_attacks(&gs);

                if (gs.hp <= 0) {
                    game_over = true;
                    gs.has_fwdump = false;
                }

                compute_fov(&gs);
                update_viewport(&gs);
                draw_map(&gs, sr, sc);
                draw_room_label(&gs, sr, sc);
                draw_hud(&gs, hud_row);
                draw_msg(&gs, msg_row);
                tx_fifo_wait_drain();
            }

            if (quit) break;

            if (game_over) {
                printf("\x1b[2J");
                int mid = gs.field_h / 2;
                int mc = (gs.field_w - 32) / 2;
                if (mc < 1) mc = 1;

                bool won = (gs.has_fwdump && gs.hp > 0);

                ui_term_cursor_position(mid - 3, mc);
                if (won) {
                    printf("%s%s+--------------------------------+%s",
                           CC("\x1b[0;7m"), CC("\x1b[32m"), RR());
                    ui_term_cursor_position(mid - 2, mc);
                    printf("%s%s|   FIRMWARE DUMP EXTRACTED!     |%s",
                           CC("\x1b[0;7m"), CC("\x1b[32m"), RR());
                    ui_term_cursor_position(mid - 1, mc);
                    printf("%s%s|     PROBE ESCAPED SAFELY!      |%s",
                           CC("\x1b[0;7m"), CC("\x1b[32m"), RR());
                    ui_term_cursor_position(mid, mc);
                    printf("%s%s+--------------------------------+%s",
                           CC("\x1b[0;7m"), CC("\x1b[32m"), RR());
                } else {
                    printf("%s%s+--------------------------------+%s",
                           CC("\x1b[0;7m"), CC("\x1b[31m"), RR());
                    ui_term_cursor_position(mid - 2, mc);
                    printf("%s%s|       PROBE  DESTROYED!        |%s",
                           CC("\x1b[0;7m"), CC("\x1b[31m"), RR());
                    ui_term_cursor_position(mid - 1, mc);
                    printf("%s%s|   The chip remains sealed...   |%s",
                           CC("\x1b[0;7m"), CC("\x1b[31m"), RR());
                    ui_term_cursor_position(mid, mc);
                    printf("%s%s+--------------------------------+%s",
                           CC("\x1b[0;7m"), CC("\x1b[31m"), RR());
                }

                ui_term_cursor_position(mid + 2, mc);
                printf("  Floor: %s%d%s  Turns: %s%d%s  Kills: %s%d%s",
                       CC("\x1b[0;36m"), gs.floor_num, RR(),
                       CC("\x1b[0;33m"), gs.turns, RR(),
                       CC("\x1b[0;31m"), gs.kills, RR());
                ui_term_cursor_position(mid + 3, mc);
                printf("  HP: %s%d%s/%d  Shield: %s%d%s",
                       gs.hp > 0 ? CC("\x1b[0;1;32m") : CC("\x1b[0;1;31m"), gs.hp, RR(), gs.max_hp,
                       CC("\x1b[0;1;36m"), gs.shield, RR());
                ui_term_cursor_position(mid + 5, mc);
                printf("  SPACE = new game   q = quit");
                tx_fifo_wait_drain();

                bool waiting = true;
                while (waiting) {
                    game_input_t wi;
                    if (game_poll_input(&wi, 0)) {
                        if (wi.type == GAME_INPUT_QUIT) { quit = true; waiting = false; }
                        if (wi.type == GAME_INPUT_ACTION) { waiting = false; }
                    }
                    busy_wait_ms(30);
                }
            } else {
                gs.floor_num++;
            }
        }
    }

    game_screen_exit();
}
