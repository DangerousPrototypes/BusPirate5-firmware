// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file crossflash.c
 * @brief Crossflash — Frogger-style bus-crossing game for Bus Pirate.
 *
 * Guide your logic probe across lanes of scrolling data buses.
 * Each lane is a different protocol signal (SPI CLK, I2C SDA, UART TX,
 * etc.) scrolling at different speeds and directions. Time your
 * crossing to avoid getting clocked! Reach the target pad at the top
 * to score. Each round the buses get faster.
 *
 * Controls: UP/DOWN/LEFT/RIGHT = move, q = quit.
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
#include "crossflash.h"

static const char* const usage[] = {
    "xflash",
    "Launch Crossflash:%s xflash",
    "",
    "Cross scrolling data buses without getting clocked!",
    "Arrow keys=move  q=quit",
};

const bp_command_def_t crossflash_def = {
    .name = "xflash",
    .description = T_HELP_CROSSFLASH,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MAX_LANES      10      // max bus lanes
#define MAX_SEGMENTS   12      // max scrolling segments per lane
#define SEG_MAX_W      8       // max segment (pulse) width
#define SEG_MIN_W      3       // min segment width
#define SEG_GAP_MIN    4       // min gap between segments
#define SEG_GAP_MAX    10      // max gap between segments
#define TICK_MS        100     // frame time
#define LABEL_W        4       // protocol label width on left margin

// Lane signal types — each has a visual character pattern and color
typedef struct {
    const char* name;       // protocol label (shown on left margin)
    char pulse_ch;          // character used for the "high" pulse
    char idle_ch;           // character used for idle/low
    const char* pulse_clr;  // ANSI color for pulse
    const char* idle_clr;   // ANSI color for idle
} lane_style_t;

// ANSI colors
static const char* const grn = "\x1b[32m";
static const char* const red = "\x1b[31m";
static const char* const ylw = "\x1b[33m";
static const char* const cyn = "\x1b[36m";
static const char* const wht = "\x1b[1;37m";
static const char* const mag = "\x1b[35m";
static const char* const rst = "\x1b[0m";
static const char* const dim = "\x1b[2m";

static const lane_style_t styles[] = {
    { "CLK ", '#', '_', ylw, dim },    // SPI Clock
    { "SDA ", '=', '_', grn, dim },    // I2C SDA
    { "SCL ", '#', '_', cyn, dim },    // I2C SCL
    { "TX  ", '~', '.', mag, dim },    // UART TX
    { "RX  ", '~', '.', red, dim },    // UART RX
    { "MOSI", '#', '_', ylw, dim },    // SPI MOSI
    { "MISO", '=', '_', grn, dim },    // SPI MISO
    { "CS  ", '_', '-', red, dim },    // Chip Select (active low)
    { "1W  ", '#', '_', cyn, dim },    // 1-Wire
    { "IRQ ", '!', '.', red, dim },    // Interrupt
};
#define STYLE_COUNT (sizeof(styles) / sizeof(styles[0]))
#define MAX_TARGETS    5       // max target columns

// A segment is a "pulse" or "packet" moving across a lane
typedef struct {
    int8_t x;       // left edge column position (range ~-16 to ~125)
    uint8_t w;      // width in columns (3-8)
} segment_t;

// A lane is one horizontal row of scrolling traffic
typedef struct {
    uint8_t screen_row;     // which screen row this lane occupies
    int8_t dir;             // +1 = right, -1 = left
    uint8_t speed;          // move every N ticks (lower = faster)
    uint8_t tick_counter;   // counts up to speed
    uint8_t style_idx;      // index into styles[]
    uint8_t seg_count;
    segment_t segs[MAX_SEGMENTS];
} lane_t;

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef struct {
    int field_w;
    int field_h;
    int status_row;
    int player_row;
    int player_col;
    int score;
    int hi_score;
    int level;
    int lives;
    lane_t lanes[MAX_LANES];
    int lane_count;
    int target_cols[MAX_TARGETS];
    bool target_filled[MAX_TARGETS];
    int target_count;
    int start_row;
    int target_row;
} crossflash_state_t;



// ---------------------------------------------------------------------------
// Lane setup
// ---------------------------------------------------------------------------

static void spawn_segments(crossflash_state_t* gs, lane_t* lane) {
    lane->seg_count = 0;
    // Fill the lane with segments spaced across the field
    int x = -(int)(game_rng_next() % 10);
    while (x < gs->field_w + 20 && lane->seg_count < MAX_SEGMENTS) {
        segment_t* s = &lane->segs[lane->seg_count++];
        s->w = SEG_MIN_W + (int)(game_rng_next() % (SEG_MAX_W - SEG_MIN_W + 1));
        s->x = (int8_t)x;
        x += s->w + SEG_GAP_MIN + (int)(game_rng_next() % (SEG_GAP_MAX - SEG_GAP_MIN + 1));
    }
}

static void init_lanes(crossflash_state_t* gs) {
    // Layout (top to bottom):
    //   row 1          = target pads
    //   rows 2..N      = bus lanes
    //   row N+1        = safe zone (dashes)
    //   row N+2        = start zone (equals)
    //   row field_h    = status bar
    // Use ALL available rows — no wasted space.
    gs->target_row = 1;
    // Rows available for lanes = total - target(1) - safe(1) - start(1) - status(1)
    gs->lane_count = gs->field_h - 4;
    if (gs->lane_count > MAX_LANES) gs->lane_count = MAX_LANES;
    if (gs->lane_count < 3) gs->lane_count = 3;

    gs->start_row = gs->target_row + 1 + gs->lane_count + 1;  // safe row is start_row - 1

    for (int i = 0; i < gs->lane_count; i++) {
        lane_t* l = &gs->lanes[i];
        l->screen_row = gs->target_row + 1 + i; // row 2, 3, 4...
        l->dir = (game_rng_next() & 1) ? 1 : -1;
        // Speed: 1 (fast) to 4 (slow), biased by level
        int max_spd = 5 - gs->level;
        if (max_spd < 1) max_spd = 1;
        l->speed = 1 + (int)(game_rng_next() % (max_spd > 3 ? 3 : max_spd));
        l->tick_counter = 0;
        l->style_idx = (int)(game_rng_next() % STYLE_COUNT);
        spawn_segments(gs, l);
    }
}

static void init_targets(crossflash_state_t* gs) {
    gs->target_count = 3 + (gs->field_w > 60 ? 2 : 0);
    if (gs->target_count > MAX_TARGETS) gs->target_count = MAX_TARGETS;
    int spacing = gs->field_w / (gs->target_count + 1);
    for (int i = 0; i < gs->target_count; i++) {
        gs->target_cols[i] = spacing * (i + 1);
        gs->target_filled[i] = false;
    }
}

// ---------------------------------------------------------------------------
// Scrolling
// ---------------------------------------------------------------------------

static void scroll_lanes(crossflash_state_t* gs) {
    for (int i = 0; i < gs->lane_count; i++) {
        lane_t* l = &gs->lanes[i];
        l->tick_counter++;
        if (l->tick_counter < l->speed) continue;
        l->tick_counter = 0;

        for (int s = 0; s < l->seg_count; s++) {
            l->segs[s].x += l->dir;
        }

        // Wrap segments that go fully off-screen
        for (int s = 0; s < l->seg_count; s++) {
            segment_t* seg = &l->segs[s];
            if (l->dir > 0 && seg->x > (int8_t)(gs->field_w + 5)) {
                // Wrap to left
                int nx = -(seg->w + (int)(game_rng_next() % 8));
                seg->x = (int8_t)(nx < -128 ? -128 : nx);
            } else if (l->dir < 0 && seg->x + seg->w < -5) {
                // Wrap to right
                int nx = gs->field_w + (int)(game_rng_next() % 8);
                seg->x = (int8_t)(nx > 127 ? 127 : nx);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------

// Check if a screen position is on a pulse segment
static bool is_on_pulse(crossflash_state_t* gs, int row, int col) {
    for (int i = 0; i < gs->lane_count; i++) {
        if (gs->lanes[i].screen_row != row) continue;
        for (int s = 0; s < gs->lanes[i].seg_count; s++) {
            segment_t* seg = &gs->lanes[i].segs[s];
            if (col >= seg->x && col < seg->x + seg->w) {
                return true;
            }
        }
        break;
    }
    return false;
}

// Check if player is on a lane row (not safe zone)
static bool is_lane_row(crossflash_state_t* gs, int row) {
    for (int i = 0; i < gs->lane_count; i++) {
        if (gs->lanes[i].screen_row == row) return true;
    }
    return false;
}

// Check if player reached a target pad
static int check_target(crossflash_state_t* gs) {
    if (gs->player_row != gs->target_row) return -1;
    for (int i = 0; i < gs->target_count; i++) {
        if (!gs->target_filled[i] &&
            gs->player_col >= gs->target_cols[i] - 2 &&
            gs->player_col <= gs->target_cols[i] + 2) {
            return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Drawing — line-buffer approach for efficiency
// ---------------------------------------------------------------------------

// Color index → ANSI string (set once, emit per span)
#define CLR_DIM    0
#define CLR_YLW    1
#define CLR_GRN    2
#define CLR_RED    3
#define CLR_CYN    4
#define CLR_MAG    5
#define CLR_WHT    6

static const char* const clr_table[] = {
    "\x1b[2m",      // 0 dim
    "\x1b[33m",     // 1 yellow
    "\x1b[32m",     // 2 green
    "\x1b[31m",     // 3 red
    "\x1b[36m",     // 4 cyan
    "\x1b[35m",     // 5 magenta
    "\x1b[1;37m",   // 6 bold white
};

// Map lane style pulse_clr pointer to color index
static uint8_t style_to_cidx(const char* clr) {
    if (clr == ylw) return CLR_YLW;
    if (clr == grn) return CLR_GRN;
    if (clr == red) return CLR_RED;
    if (clr == cyn) return CLR_CYN;
    if (clr == mag) return CLR_MAG;
    return CLR_DIM;
}

// Emit a row buffer from stack arrays: batch consecutive same-color characters
static void emit_row(int row, char* rch, uint8_t* rclr, int cols) {
    ui_term_cursor_position(row, 1);
    uint8_t cur_clr = 255; // force first color emit
    int span_start = 0;
    for (int c = 0; c < cols; c++) {
        if (rclr[c] != cur_clr) {
            // Flush previous span
            if (c > span_start) {
                printf("%.*s", c - span_start, &rch[span_start]);
            }
            cur_clr = rclr[c];
            printf("%s", clr_table[cur_clr]);
            span_start = c;
        }
    }
    // Flush final span
    if (cols > span_start) {
        printf("%.*s", cols - span_start, &rch[span_start]);
    }
    printf("\x1b[0m\x1b[K"); // reset + clear to EOL
}

static void draw_frame(crossflash_state_t* gs) {
    int w = gs->field_w;
    if (w > 80) w = 80;
    char rch[80];
    uint8_t rclr[80];

    // --- Target row (row 1): safe zone with landing pads ---
    for (int c = 0; c < w; c++) {
        bool is_pad = false;
        for (int t = 0; t < gs->target_count; t++) {
            int col1 = gs->target_cols[t] - 3;  // 0-indexed
            int col2 = gs->target_cols[t] + 1;
            if (c >= col1 && c <= col2) {
                is_pad = true;
                if (gs->target_filled[t]) {
                    rch[c] = '@';
                    rclr[c] = CLR_GRN;
                } else {
                    rch[c] = '#';
                    rclr[c] = CLR_YLW;
                }
                break;
            }
        }
        if (!is_pad) {
            rch[c] = '~';
            rclr[c] = CLR_DIM;
        }
    }
    emit_row(gs->target_row, rch, rclr, w);

    // --- Bus lanes ---
    for (int i = 0; i < gs->lane_count; i++) {
        lane_t* l = &gs->lanes[i];
        const lane_style_t* st = &styles[l->style_idx];
        uint8_t pulse_ci = style_to_cidx(st->pulse_clr);

        // Label columns 0..3
        const char* nm = st->name;
        for (int c = 0; c < LABEL_W && c < w; c++) {
            rch[c] = nm[c];
            rclr[c] = pulse_ci;
        }
        // Content columns LABEL_W..w-1
        for (int c = LABEL_W; c < w; c++) {
            bool on_pulse = false;
            int col1 = c + 1; // 1-based column
            for (int s = 0; s < l->seg_count; s++) {
                if (col1 >= l->segs[s].x && col1 < l->segs[s].x + l->segs[s].w) {
                    on_pulse = true;
                    break;
                }
            }
            if (on_pulse) {
                rch[c] = st->pulse_ch;
                rclr[c] = pulse_ci;
            } else {
                rch[c] = st->idle_ch;
                rclr[c] = CLR_DIM;
            }
        }
        emit_row(l->screen_row, rch, rclr, w);
    }

    // --- Safe row ---
    int safe_row = gs->start_row - 1;
    for (int c = 0; c < w; c++) {
        rch[c] = '-';
        rclr[c] = CLR_DIM;
    }
    emit_row(safe_row, rch, rclr, w);

    // --- Start row ---
    for (int c = 0; c < w; c++) {
        rch[c] = '=';
        rclr[c] = CLR_DIM;
    }
    emit_row(gs->start_row, rch, rclr, w);

    // --- Player sprite: <@> (3 chars, bright white, drawn LAST) ---
    {
        int pr = gs->player_row;
        int pc = gs->player_col;
        ui_term_cursor_position(pr, (pc > 1) ? pc - 1 : 1);
        printf("\x1b[1;37m");
        if (pc > 1) printf("<");
        printf("@");
        if (pc < w) printf(">");
        printf("\x1b[0m");
    }

    // --- Status bar ---
    ui_term_cursor_position(gs->status_row, 1);
    printf(" \x1b[1;37mXFLASH\x1b[0m S:\x1b[33m%05d\x1b[0m"
           " Hi:\x1b[31m%05d\x1b[0m Lv:\x1b[36m%d\x1b[0m"
           " x\x1b[32m%d\x1b[0m ^v<>=move q=quit\x1b[K",
           gs->score, gs->hi_score, gs->level, gs->lives);
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void crossflash_handler(struct command_result* res) {
    res->error = false;

    crossflash_state_t gs = {0};
    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 40) gs.field_w = 80;
    if (gs.field_h < 14) gs.field_h = 24;

    game_rng_seed();

    // Enter alt screen (scroll region set after init_lanes determines status_row)
    game_screen_enter(0);

    bool quit = false;

    while (!quit) {
        // --- New game ---
        gs.score = 0;
        gs.level = 1;
        gs.lives = 3;

        init_lanes(&gs);
        init_targets(&gs);
        gs.status_row = gs.start_row + 1;  // status bar right below start zone

        // Set scroll region to protect status bar
        game_set_scroll_region(gs.status_row - 1);

        gs.player_row = gs.start_row;
        gs.player_col = gs.field_w / 2;

        printf("\x1b[2J");

        // Title flash
        ui_term_cursor_position(gs.field_h / 2 - 1, (gs.field_w - 28) / 2);
        printf("%s=== CROSSFLASH ===%s", cyn, rst);
        ui_term_cursor_position(gs.field_h / 2, (gs.field_w - 28) / 2);
        printf("Cross the data buses!");
        ui_term_cursor_position(gs.field_h / 2 + 1, (gs.field_w - 28) / 2);
        printf("Arrow keys to move");
        tx_fifo_wait_drain();
        busy_wait_ms(1500);
        printf("\x1b[2J");

        bool game_over = false;

        while (!game_over) {
            uint32_t tick_start = time_us_32();

            // --- Input ---
            char c;
            while (rx_fifo_try_get(&c)) {
                if (c == 'q' || c == 'Q') { game_over = true; quit = true; break; }
                // WASD support
                if (c == 'w' || c == 'W') {
                    if (gs.player_row > gs.target_row) gs.player_row--;
                } else if (c == 's' || c == 'S') {
                    if (gs.player_row < gs.start_row) gs.player_row++;
                } else if (c == 'd' || c == 'D') {
                    if (gs.player_col < gs.field_w - 1) gs.player_col++;
                } else if (c == 'a' || c == 'A') {
                    if (gs.player_col > LABEL_W + 2) gs.player_col--;
                }
                // Arrow keys
                if (c == 0x1b) {
                    char s1, s2;
                    if (rx_fifo_try_get(&s1) && s1 == '[') {
                        if (rx_fifo_try_get(&s2)) {
                            if (s2 == 'A' && gs.player_row > gs.target_row) {
                                gs.player_row--;
                            }
                            if (s2 == 'B' && gs.player_row < gs.start_row) {
                                gs.player_row++;
                            }
                            if (s2 == 'C' && gs.player_col < gs.field_w - 1) {
                                gs.player_col++;
                            }
                            if (s2 == 'D' && gs.player_col > LABEL_W + 2) {
                                gs.player_col--;
                            }
                        }
                    }
                }
            }
            if (quit) break;

            // --- Scroll bus traffic ---
            scroll_lanes(&gs);

            // --- Collision: did a pulse hit the player? ---
            if (is_lane_row(&gs, gs.player_row) && is_on_pulse(&gs, gs.player_row, gs.player_col)) {
                gs.lives--;
                // Reset to start
                gs.player_row = gs.start_row;
                gs.player_col = gs.field_w / 2;
                if (gs.lives <= 0) {
                    game_over = true;
                } else {
                    // Flash hit effect
                    ui_term_cursor_position(gs.field_h / 2, (gs.field_w - 12) / 2);
                    printf("%s** CLOCKED! **%s", red, rst);
                    tx_fifo_wait_drain();
                    busy_wait_ms(600);
                }
            }

            // --- Check target reached ---
            int t = check_target(&gs);
            if (t >= 0) {
                gs.target_filled[t] = true;
                gs.score += 100 * gs.level;
                gs.player_row = gs.start_row;
                gs.player_col = gs.field_w / 2;

                // All targets filled? Next level
                bool all_filled = true;
                for (int i = 0; i < gs.target_count; i++) {
                    if (!gs.target_filled[i]) { all_filled = false; break; }
                }
                if (all_filled) {
                    gs.level++;
                    gs.score += 500;
                    printf("\x1b[2J");
                    ui_term_cursor_position(gs.field_h / 2, (gs.field_w - 20) / 2);
                    printf("%s=== LEVEL %d ===%s", grn, gs.level, rst);
                    tx_fifo_wait_drain();
                    busy_wait_ms(1000);
                    printf("\x1b[2J");
                    init_lanes(&gs);
                    init_targets(&gs);
                    gs.status_row = gs.start_row + 1;
                    game_set_scroll_region(gs.status_row - 1);
                }
            }

            // --- Draw ---
            tx_fifo_wait_drain();
            draw_frame(&gs);

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

            ui_term_cursor_position(mr - 2, mc);
            printf("%s+==========================+%s", red, rst);
            ui_term_cursor_position(mr - 1, mc);
            printf("%s|       CLOCKED OUT!       |%s", red, rst);
            ui_term_cursor_position(mr, mc);
            printf("%s|   The bus got you.       |%s", red, rst);
            ui_term_cursor_position(mr + 1, mc);
            printf("%s|   Score: %s%-5d%s  Lv: %s%d%s   |%s",
                   red, ylw, gs.score, red, cyn, gs.level, red, rst);
            ui_term_cursor_position(mr + 2, mc);
            printf("%s+==========================+%s", red, rst);
            ui_term_cursor_position(mr + 4, (gs.field_w - 28) / 2);
            printf("SPACE = cross again   q = quit");
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
