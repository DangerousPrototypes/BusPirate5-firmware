// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file pcbrun.c
 * @brief PCB Run — side-scrolling jump game for Bus Pirate.
 *
 * You are a probe tip racing across an endless PCB trace.
 * Jump over electronics components! Speed increases over time.
 * Controls: SPACE or UP = jump, q = quit.
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
#include "pcbrun.h"

static const char* const usage[] = {
    "pcbrun",
    "Launch PCB Run:%s pcbrun",
    "",
    "Jump over electronics components on an endless PCB trace!",
    "SPACE/UP=jump  q=quit",
};

const bp_command_def_t pcbrun_def = {
    .name = "pcbrun",
    .description = T_HELP_PCBRUN,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Obstacle types — electronics components
// ---------------------------------------------------------------------------
// Each obstacle has a name, ASCII art rows (drawn bottom-up from the ground),
// a width (in chars), and a height (in rows above ground).

#define MAX_OBS_W  8
#define MAX_OBS_H  4

typedef struct {
    const char* name;
    uint8_t w;       // width in chars
    uint8_t h;       // height in rows above ground line
    // Art rows, index 0 = top row, stored as strings
    const char* art[MAX_OBS_H];
} obs_type_t;

static const obs_type_t obs_types[] = {
    // Resistor — small, 1 high
    { "Resistor", 6, 1, { "-[//]-", NULL, NULL, NULL } },
    // Capacitor — 1 high
    { "Capacitor", 4, 1, { "-||-", NULL, NULL, NULL } },
    // Diode — 1 high, arrow shape
    { "Diode", 5, 1, { "-|>|-", NULL, NULL, NULL } },
    // DIP chip — 2 high, wide
    { "DIP IC", 8, 2, { "[DIP-16]", "[______]", NULL, NULL } },
    // Electrolytic cap — 1 high
    { "Elec Cap", 4, 1, { "-|[-", NULL, NULL, NULL } },
    // Crystal oscillator — 1 high
    { "Crystal", 6, 1, { "-|[]|-", NULL, NULL, NULL } },
    // MOSFET — 2 high
    { "MOSFET", 5, 2, { " FET ", "[|||]", NULL, NULL } },
    // Heatsink — 3 high, wide
    { "Heatsink", 6, 3, { "/\\/\\/\\", "||||||", "[====]", NULL } },
    // LED — 1 high
    { "LED", 5, 1, { "-|>]-", NULL, NULL, NULL } },
    // Fuse — 1 high
    { "Fuse", 4, 1, { "-//-", NULL, NULL, NULL } },
    // Inductor — 1 high
    { "Inductor", 6, 1, { "-^^^^-", NULL, NULL, NULL } },
    // Relay — 3 high, wide
    { "Relay", 7, 3, { "[RELAY]", "| o-o |", "[_____]", NULL } },
    // Op-amp — 1 high
    { "Op-amp", 4, 1, { "=|>-", NULL, NULL, NULL } },
    // Zener diode — 1 high
    { "Zener", 7, 1, { "-|<z>|-", NULL, NULL, NULL } },
    // Switch — 1 high (open contacts)
    { "Switch", 4, 1, { "- /-", NULL, NULL, NULL } },
    // Potentiometer — 2 high
    { "Pot", 7, 2, { "   |   ", "-/\\/\\/-", NULL, NULL } },
};
#define OBS_TYPE_COUNT (sizeof(obs_types) / sizeof(obs_types[0]))

// ---------------------------------------------------------------------------
// Scrolling obstacle queue
// ---------------------------------------------------------------------------
#define MAX_OBSTACLES 12

typedef struct {
    int x;           // column position (leftward scrolling)
    uint8_t type;    // index into obs_types
    bool active;
} obstacle_t;

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
#define PLAYER_COL    8    // fixed column for player
#define GROUND_CHAR   '='
#define JUMP_HEIGHT   5    // max rows above ground
#define JUMP_FRAMES   12   // total frames for jump arc
#define LINE_BUF_MAX  200

typedef struct {
    obstacle_t obstacles[MAX_OBSTACLES];
    int ground_row;
    int field_w;
    int player_y;
    int jump_frame;
    int score;
    int hi_score;
    int speed_ticks;
    int frames_since_obs;
    int min_gap;
    int jump_curve[JUMP_FRAMES];
    char line_buf[LINE_BUF_MAX];
    uint8_t line_who[LINE_BUF_MAX];
} pcbrun_state_t;

static void init_jump_curve(pcbrun_state_t* gs) {
    // Symmetric parabola peaking at JUMP_HEIGHT
    // y = JUMP_HEIGHT * (1 - ((t - mid) / mid)^2)
    int mid = JUMP_FRAMES / 2;
    for (int i = 0; i < JUMP_FRAMES; i++) {
        int d = i - mid; // distance from apex
        // Integer approximation: h = H - H*d*d / (mid*mid)
        int h = JUMP_HEIGHT - (JUMP_HEIGHT * d * d) / (mid * mid);
        if (h < 0) h = 0;
        gs->jump_curve[i] = h;
    }
}

static void init_obstacles(pcbrun_state_t* gs) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        gs->obstacles[i].active = false;
    }
    gs->frames_since_obs = 0;
}

static void spawn_obstacle(pcbrun_state_t* gs) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!gs->obstacles[i].active) {
            gs->obstacles[i].active = true;
            gs->obstacles[i].x = gs->field_w + 1;
            gs->obstacles[i].type = game_rng_next() % OBS_TYPE_COUNT;
            gs->frames_since_obs = 0;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------
static bool check_collision(pcbrun_state_t* gs) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!gs->obstacles[i].active) continue;
        const obs_type_t* ot = &obs_types[gs->obstacles[i].type];
        int ox = gs->obstacles[i].x;

        // Player occupies column PLAYER_COL, rows ground_row-player_y (head)
        // to ground_row-1 (we'll say player is 2 rows tall: head + body)
        // Actually let's keep it simple: player is 1 cell at (ground_row - player_y)
        // Obstacle spans columns [ox .. ox+w-1], rows [ground_row-h .. ground_row-1]

        // Horizontal overlap?
        if (PLAYER_COL < ox || PLAYER_COL >= ox + (int)ot->w) continue;

        // Vertical overlap? Player head at row (ground_row - 1 - player_y)
        // Obstacle top = ground_row - ot->h, bottom = ground_row - 1
        int player_row = gs->ground_row - 1 - gs->player_y;
        int obs_top = gs->ground_row - (int)ot->h;
        int obs_bot = gs->ground_row - 1;

        if (player_row >= obs_top && player_row <= obs_bot) {
            return true;
        }
        // Also check player body (1 row below head) if airborne
        if (gs->player_y >= 2) {
            int body_row = player_row + 1;
            if (body_row >= obs_top && body_row <= obs_bot) {
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void draw_frame(pcbrun_state_t* gs, int status_row) {
    bool color = system_config.terminal_ansi_color;
    const char* rst = color ? "\x1b[0m" : "";
    const char* grn = color ? "\x1b[1;32m" : "";
    const char* ylw = color ? "\x1b[1;33m" : "";
    const char* cyn = color ? "\x1b[36m" : "";
    const char* red = color ? "\x1b[1;31m" : "";
    const char* wht = color ? "\x1b[1;37m" : "";

    // Play area bounds
    int top_row = gs->ground_row - JUMP_HEIGHT - 2;
    if (top_row < 2) top_row = 2;

    // Line buffer: index 0 = screen column 1.  Use 0-based throughout.
    int w = gs->field_w;
    if (w > LINE_BUF_MAX) w = LINE_BUF_MAX;

    int pr = gs->ground_row - 1 - gs->player_y; // player head screen row

    for (int r = top_row; r < gs->ground_row; r++) {
        // Fill with spaces
        memset(gs->line_buf, ' ', w);
        memset(gs->line_who, 0, w);

        // Stamp obstacles (obstacle x is 1-based screen column, convert to 0-based)
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            if (!gs->obstacles[i].active) continue;
            const obs_type_t* ot = &obs_types[gs->obstacles[i].type];
            int ox = gs->obstacles[i].x; // 1-based screen column

            for (int row = 0; row < (int)ot->h; row++) {
                if (!ot->art[row]) continue;
                int screen_row = gs->ground_row - (int)ot->h + row;
                if (screen_row != r) continue;
                int len = (int)strlen(ot->art[row]);
                for (int ch = 0; ch < len; ch++) {
                    int bi = (ox - 1) + ch; // 0-based buffer index
                    if (bi >= 0 && bi < w) {
                        gs->line_buf[bi] = ot->art[row][ch];
                        gs->line_who[bi] = 1;
                    }
                }
            }
        }

        // Stamp player (PLAYER_COL is 1-based screen column)
        int pi = PLAYER_COL - 1; // 0-based buffer index
        if (r == pr && pi >= 0 && pi < w) {
            gs->line_buf[pi] = (gs->jump_frame >= 0) ? '^' : '>';
            gs->line_who[pi] = 2;
        }
        if (gs->player_y >= 2 && r == pr + 1 && pi >= 0 && pi < w) {
            gs->line_buf[pi] = '|';
            gs->line_who[pi] = 2;
        }

        // Render: position at column 1, print full line, clear remainder
        ui_term_cursor_position(r, 1);
        for (int c = 0; c < w; c++) {
            char ch = gs->line_buf[c];
            if (gs->line_who[c] == 2) {
                printf("%s%c%s", grn, ch, rst);
            } else if (gs->line_who[c] == 1) {
                printf("%s%c%s", ylw, ch, rst);
            } else {
                printf(" ");
            }
        }
        printf("\x1b[K"); // clear to end of line (no leftover chars)
    }

    // Ground line: single pass, green marker at player column
    ui_term_cursor_position(gs->ground_row, 1);
    for (int c = 0; c < w; c++) {
        if (c == PLAYER_COL - 1) {
            printf("%s%c%s", grn, GROUND_CHAR, rst);
        } else {
            printf("%s%c%s", cyn, GROUND_CHAR, rst);
        }
    }
    printf("\x1b[K");

    // Status bar
    ui_term_cursor_position(status_row, 1);
    printf(" %sPCB Run%s  Score: %s%06d%s  Hi: %s%06d%s  Speed: %d    SPACE/UP=jump  q=quit",
           wht, rst, ylw, gs->score, rst, red, gs->hi_score, rst,
           (100 - gs->speed_ticks) / 5 + 1);
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void pcbrun_handler(struct command_result* res) {
    if (bp_cmd_help_check(&pcbrun_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;
    if (term_rows < 12 || term_cols < 40) {
        printf("Terminal too small (need at least 40x12)\r\n");
        return;
    }

    pcbrun_state_t gs = {0};

    gs.field_w = term_cols;
    gs.ground_row = term_rows - 3;
    int status_row = term_rows - 1;

    init_jump_curve(&gs);

    game_screen_enter(0);

    game_rng_seed();

    bool quit = false;

    while (!quit) {
        // New game
        gs.player_y = 0;
        gs.jump_frame = -1;
        gs.score = 0;
        gs.speed_ticks = 100;  // ms per tick (decreases over time)
        gs.min_gap = 18;
        init_obstacles(&gs);

        printf("\x1b[2J");

        // Initial ground
        {
            bool color = system_config.terminal_ansi_color;
            const char* cyn = color ? "\x1b[36m" : "";
            const char* rst = color ? "\x1b[0m" : "";
            ui_term_cursor_position(gs.ground_row, 1);
            printf("%s", cyn);
            for (int c = 0; c < gs.field_w; c++) printf("%c", GROUND_CHAR);
            printf("%s", rst);
        }

        bool alive = true;

        // Countdown
        for (int cd = 3; cd >= 1; cd--) {
            ui_term_cursor_position(gs.ground_row - 3, gs.field_w / 2 - 2);
            printf("\x1b[1;33m %d... \x1b[0m", cd);
            tx_fifo_wait_drain();
            busy_wait_ms(600);
            // Drain input during countdown
            char drain;
            while (rx_fifo_try_get(&drain)) {}
        }
        ui_term_cursor_position(gs.ground_row - 3, gs.field_w / 2 - 2);
        printf("      "); // clear countdown

        while (alive && !quit) {
            uint32_t tick_start = time_us_32();

            // --- Process input ---
            char c;
            while (rx_fifo_try_get(&c)) {
                if (c == 'q' || c == 'Q') { quit = true; break; }

                bool is_jump = (c == ' ' || c == 'w' || c == 'W');
                if (c == 0x1b) {
                    char s1, s2;
                    busy_wait_ms(2);
                    if (rx_fifo_try_get(&s1) && s1 == '[') {
                        busy_wait_ms(2);
                        if (rx_fifo_try_get(&s2)) {
                            if (s2 == 'A') is_jump = true; // up arrow
                        }
                    }
                }

                if (is_jump && gs.jump_frame < 0 && gs.player_y == 0) {
                    gs.jump_frame = 0;
                }
            }
            if (quit) break;

            // --- Update jump ---
            if (gs.jump_frame >= 0) {
                gs.player_y = gs.jump_curve[gs.jump_frame];
                gs.jump_frame++;
                if (gs.jump_frame >= JUMP_FRAMES) {
                    gs.jump_frame = -1;
                    gs.player_y = 0;
                }
            }

            // --- Scroll obstacles left ---
            for (int i = 0; i < MAX_OBSTACLES; i++) {
                if (!gs.obstacles[i].active) continue;
                gs.obstacles[i].x--;
                // Deactivate if fully off-screen left
                if (gs.obstacles[i].x + (int)obs_types[gs.obstacles[i].type].w < 0) {
                    gs.obstacles[i].active = false;
                    gs.score += 10;
                }
            }

            // --- Spawn new obstacles ---
            gs.frames_since_obs++;
            if (gs.frames_since_obs >= gs.min_gap) {
                // Random chance to spawn, increasing with gap
                if ((int)(game_rng_next() % 10) < 3 || gs.frames_since_obs > gs.min_gap + 10) {
                    spawn_obstacle(&gs);
                }
            }

            // --- Scoring ---
            gs.score++;

            // --- Speed up over time ---
            if (gs.score % 50 == 0 && gs.speed_ticks > 35) {
                gs.speed_ticks -= 5;
            }
            if (gs.score % 100 == 0 && gs.min_gap > 10) {
                gs.min_gap--;
            }

            // --- Draw BEFORE collision check so crash frame is accurate ---
            tx_fifo_wait_drain();
            draw_frame(&gs, status_row);

            // --- Collision check ---
            if (check_collision(&gs)) {
                alive = false;
                break;
            }

            // --- Timing ---
            uint32_t elapsed = (time_us_32() - tick_start) / 1000;
            if ((int)elapsed < gs.speed_ticks) {
                busy_wait_ms(gs.speed_ticks - (int)elapsed);
            }
        }

        if (!quit) {
            // Game over
            if (gs.score > gs.hi_score) gs.hi_score = gs.score;

            bool color = system_config.terminal_ansi_color;
            const char* rst = color ? "\x1b[0m" : "";
            const char* red = color ? "\x1b[1;31m" : "";

            int mid = gs.ground_row / 2 - 2;
            int cx = (gs.field_w - 20) / 2;
            if (cx < 1) cx = 1;
            if (mid < 2) mid = 2;

            ui_term_cursor_position(mid, cx);
            printf("%s  *** CRASH! ***  %s", red, rst);
            ui_term_cursor_position(mid + 1, cx);
            printf("  Score: %-6d     ", gs.score);
            ui_term_cursor_position(mid + 2, cx);
            printf("  Player Y: %-2d  Row: %-3d", gs.player_y, gs.ground_row - 1 - gs.player_y);
            if (gs.score >= gs.hi_score) {
                ui_term_cursor_position(mid + 3, cx);
                printf("%s  NEW HIGH!  %s", color ? "\x1b[1;33m" : "", rst);
            }
            ui_term_cursor_position(mid + 4, cx);
            printf("  r=retry  q=quit   ");

            tx_fifo_wait_drain();

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
