// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file sigride.c
 * @brief Signal Rider — oscilloscope surfing game for Bus Pirate.
 *
 * Ride a scrolling waveform on a scope display. The signal scrolls
 * left continuously — you move left/right/up/down to stay on the
 * waveform and dodge hazards: noise glitches (spikes), ground faults,
 * ringing, and EMI bursts. Collect clean edges for bonus points.
 *
 * The scope grid scrolls behind you. Three signal lanes: HIGH, MID
 * (tri-state), LOW. The waveform transitions between them and you
 * must follow it or take damage.
 *
 * Controls: UP/DOWN = change lane, LEFT/RIGHT = fine position, q = quit.
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
#include "sigride.h"

static const char* const usage[] = {
    "sigride",
    "Launch Signal Rider:%s sigride",
    "",
    "Surf a scrolling oscilloscope waveform!",
    "UP/DOWN=change lane  LEFT/RIGHT=nudge  q=quit",
};

const bp_command_def_t sigride_def = {
    .name = "sigride",
    .description = T_HELP_SIGRIDE,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define SCOPE_MAX_W   120     // max columns we track
#define LANE_HIGH     0
#define LANE_MID      1
#define LANE_LOW      2
#define LANE_COUNT    3

#define HAZARD_NONE   0
#define HAZARD_NOISE  1       // spike — must not be on this column
#define HAZARD_GFAULT 2       // ground fault — LOW lane becomes deadly
#define HAZARD_RING   3       // ringing — HIGH lane oscillates
#define HAZARD_EMI    4       // EMI burst — all lanes dangerous, duck!
#define HAZARD_BONUS  5       // clean edge bonus pickup

#define PLAYER_COL_OFFSET  10 // player's default column from left
#define TICK_MS_INIT  90
#define TICK_MS_MIN   35
#define MAX_LIVES     3

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
    int field_w;        // terminal columns
    int field_h;        // terminal rows
    // Lane geometry
    int lane_row[LANE_COUNT]; // center row of each lane
    int lane_h;               // height of each lane band
    int ground_row;           // row for ground reference
    int status_row;
    // Waveform: what lane the signal is on at each column
    uint8_t wave_lane[SCOPE_MAX_W];    // 0=HIGH, 1=MID, 2=LOW
    uint8_t wave_hazard[SCOPE_MAX_W];  // hazard type at each column
    // Player
    int player_col;     // screen column (1-based)
    int player_lane;    // which lane player is on
    int lives;
    int score;
    int hi_score;
    int invuln_frames;  // invulnerability after hit
    int tick_ms;
} sigride_state_t;

// ---------------------------------------------------------------------------
// Waveform generation
// ---------------------------------------------------------------------------

// Generate a new column at the right edge
static void gen_wave_column(sigride_state_t* gs, int col) {
    // The signal tends to stay on its current lane but transitions
    int prev = (col > 0) ? gs->wave_lane[col - 1] : LANE_MID;
    int r = (int)(game_rng_next() % 100);

    if (r < 12) {
        // Transition up
        gs->wave_lane[col] = (prev > 0) ? prev - 1 : prev;
    } else if (r < 24) {
        // Transition down
        gs->wave_lane[col] = (prev < LANE_LOW) ? prev + 1 : prev;
    } else {
        // Stay
        gs->wave_lane[col] = prev;
    }

    // Hazards
    int hr = (int)(game_rng_next() % 100);
    if (hr < 4) {
        gs->wave_hazard[col] = HAZARD_NOISE;
    } else if (hr < 7) {
        gs->wave_hazard[col] = HAZARD_GFAULT;
    } else if (hr < 10) {
        gs->wave_hazard[col] = HAZARD_RING;
    } else if (hr < 12) {
        gs->wave_hazard[col] = HAZARD_EMI;
    } else if (hr < 16) {
        gs->wave_hazard[col] = HAZARD_BONUS;
    } else {
        gs->wave_hazard[col] = HAZARD_NONE;
    }
}

static void init_waveform(sigride_state_t* gs) {
    gs->wave_lane[0] = LANE_MID;
    gs->wave_hazard[0] = HAZARD_NONE;
    for (int c = 1; c < gs->field_w; c++) {
        gen_wave_column(gs, c);
    }
}

// Scroll waveform left by 1, generate new rightmost column
static void scroll_waveform(sigride_state_t* gs) {
    memmove(&gs->wave_lane[0], &gs->wave_lane[1], (gs->field_w - 1) * sizeof(gs->wave_lane[0]));
    memmove(&gs->wave_hazard[0], &gs->wave_hazard[1], (gs->field_w - 1) * sizeof(gs->wave_hazard[0]));
    gen_wave_column(gs, gs->field_w - 1);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// Get the character for the waveform at a given column
static void get_wave_char(sigride_state_t* gs, int col, int row, char* ch, const char** color) {
    if (col < 0 || col >= gs->field_w) { *ch = ' '; *color = rst; return; }

    int sig_lane = gs->wave_lane[col];
    int sig_row = gs->lane_row[sig_lane];
    int hazard = gs->wave_hazard[col];

    // Check if this row is the signal trace
    if (row == sig_row) {
        // Signal trace character
        switch (hazard) {
            case HAZARD_NOISE:
                *ch = '~';
                *color = red;
                return;
            case HAZARD_GFAULT:
                *ch = 'G';
                *color = red;
                return;
            case HAZARD_RING:
                *ch = 'W';
                *color = ylw;
                return;
            case HAZARD_EMI:
                *ch = '#';
                *color = mag;
                return;
            case HAZARD_BONUS:
                *ch = '+';
                *color = grn;
                return;
            default:
                *ch = '_';
                *color = cyn;
                return;
        }
    }

    // Transition edges: vertical lines between lanes
    if (col > 0) {
        int prev_lane = gs->wave_lane[col - 1];
        if (prev_lane != sig_lane) {
            int top_row = gs->lane_row[prev_lane < sig_lane ? prev_lane : sig_lane];
            int bot_row = gs->lane_row[prev_lane < sig_lane ? sig_lane : prev_lane];
            if (row > top_row && row < bot_row) {
                *ch = '|';
                *color = cyn;
                return;
            }
            if (row == top_row || row == bot_row) {
                *ch = '+';
                *color = cyn;
                return;
            }
        }
    }

    // Hazard zone highlighting
    if (hazard == HAZARD_NOISE && (row == sig_row - 1 || row == sig_row + 1)) {
        *ch = '^';
        *color = red;
        return;
    }
    if (hazard == HAZARD_EMI) {
        // EMI fills the whole column
        if (row >= gs->lane_row[LANE_HIGH] && row <= gs->lane_row[LANE_LOW]) {
            *ch = ':';
            *color = dim;
            return;
        }
    }

    // Scope grid dots
    if ((col % 10) == 0 && (row % 4) == 0) {
        *ch = '.';
        *color = dim;
        return;
    }

    *ch = ' ';
    *color = rst;
}

static void draw_frame(sigride_state_t* gs) {
    // Draw game area row by row
    for (int row = 1; row <= gs->ground_row; row++) {
        ui_term_cursor_position(row, 1);

        // Lane label at left margin
        if (row == gs->lane_row[LANE_HIGH]) {
            printf("%sHI%s", ylw, rst);
        } else if (row == gs->lane_row[LANE_MID]) {
            printf("%sZ %s", dim, rst);
        } else if (row == gs->lane_row[LANE_LOW]) {
            printf("%sLO%s", grn, rst);
        } else {
            printf("  ");
        }

        // Draw columns 3..field_w
        for (int col = 2; col < gs->field_w; col++) {
            // Is the player here?
            if (col == gs->player_col && row == gs->lane_row[gs->player_lane]) {
                if (gs->invuln_frames > 0 && (gs->invuln_frames & 1)) {
                    printf(" ");  // blink when invulnerable
                } else {
                    printf("%s@%s", wht, rst);
                }
                continue;
            }

            char ch;
            const char* color;
            get_wave_char(gs, col, row, &ch, &color);
            if (ch != ' ') {
                printf("%s%c%s", color, ch, rst);
            } else {
                printf(" ");
            }
        }
        printf("\x1b[K");
    }

    // Ground reference line
    ui_term_cursor_position(gs->ground_row + 1, 1);
    for (int c = 0; c < gs->field_w; c++) {
        printf("%s-%s", dim, rst);
    }

    // Status bar
    ui_term_cursor_position(gs->status_row, 1);
    printf(" %sSIG RIDER%s S:%s%05d%s Hi:%s%05d%s x%s%d%s Spd:%s%d%s ^v=lane <>=nudge q=quit",
           wht, rst, ylw, gs->score, rst, red, gs->hi_score, rst,
           grn, gs->lives, rst, cyn, (TICK_MS_INIT - gs->tick_ms) / 5 + 1, rst);
    printf("\x1b[K");
}

// ---------------------------------------------------------------------------
// Collision / pickup
// ---------------------------------------------------------------------------

static bool check_hazard(sigride_state_t* gs) {
    if (gs->player_col < 0 || gs->player_col >= gs->field_w) return false;

    int hazard = gs->wave_hazard[gs->player_col];
    int sig_lane = gs->wave_lane[gs->player_col];

    // Player must ride the waveform — if not on the signal's lane, take damage
    if (gs->player_lane != sig_lane) {
        // Off the wave — only dangerous if there's a hazard
        // (being off-wave is fine briefly, but hazards are fatal)
        switch (hazard) {
            case HAZARD_NOISE:
            case HAZARD_EMI:
                return true;  // hit by hazard while off-wave
            default:
                break;
        }
        return false;
    }

    // On the signal's lane
    switch (hazard) {
        case HAZARD_NOISE:
            return true;   // noise spike — unavoidable on this lane
        case HAZARD_GFAULT:
            return (gs->player_lane == LANE_LOW);  // ground fault only hurts LOW
        case HAZARD_RING:
            return (gs->player_lane == LANE_HIGH); // ringing only hurts HIGH
        case HAZARD_EMI:
            return true;   // EMI hurts everywhere — you must dodge the column
        case HAZARD_BONUS:
            gs->score += 25;   // collect bonus
            gs->wave_hazard[gs->player_col] = HAZARD_NONE;
            return false;
        default:
            return false;
    }
}

// Bonus for riding the wave (on correct lane)
static bool on_signal(sigride_state_t* gs) {
    if (gs->player_col < 0 || gs->player_col >= gs->field_w) return false;
    return gs->player_lane == gs->wave_lane[gs->player_col];
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void sigride_handler(struct command_result* res) {
    res->error = false;

    sigride_state_t gs = {0};

    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 40) gs.field_w = 80;
    if (gs.field_h < 16) gs.field_h = 24;
    if (gs.field_w > SCOPE_MAX_W) gs.field_w = SCOPE_MAX_W;

    // Layout: 3 lanes evenly spaced in the play area
    // Reserve row 1 for top margin, last 2 for ground+status
    gs.ground_row = gs.field_h - 3;
    gs.status_row = gs.field_h;
    int play_h = gs.ground_row - 1;  // rows 2 .. ground_row
    gs.lane_h = play_h / LANE_COUNT;
    gs.lane_row[LANE_HIGH] = 2 + gs.lane_h / 2;
    gs.lane_row[LANE_MID]  = 2 + gs.lane_h + gs.lane_h / 2;
    gs.lane_row[LANE_LOW]  = 2 + 2 * gs.lane_h + gs.lane_h / 2;

    game_rng_seed();

    game_screen_enter(gs.field_h - 1);

    bool quit = false;

    while (!quit) {
        // --- New game ---
        gs.lives = MAX_LIVES;
        gs.score = 0;
        gs.player_lane = LANE_MID;
        gs.player_col = PLAYER_COL_OFFSET;
        gs.tick_ms = TICK_MS_INIT;
        gs.invuln_frames = 0;

        init_waveform(&gs);
        printf("\x1b[2J");

        // Title flash
        ui_term_cursor_position(gs.field_h / 2, (gs.field_w - 30) / 2);
        printf("%s~~~ SIGNAL RIDER ~~~%s", cyn, rst);
        ui_term_cursor_position(gs.field_h / 2 + 1, (gs.field_w - 30) / 2);
        printf("  Ride the waveform! GO!");
        tx_fifo_wait_drain();
        busy_wait_ms(1200);
        printf("\x1b[2J");

        bool game_over = false;
        int frame = 0;

        while (!game_over) {
            uint32_t tick_start = time_us_32();

            // --- Input ---
            char c;
            while (rx_fifo_try_get(&c)) {
                if (c == 'q' || c == 'Q') { game_over = true; quit = true; break; }
                if (c == 0x1b) {
                    char s1, s2;
                    if (rx_fifo_try_get(&s1) && s1 == '[') {
                        if (rx_fifo_try_get(&s2)) {
                            if (s2 == 'A') { // UP — lane up
                                if (gs.player_lane > LANE_HIGH) gs.player_lane--;
                            }
                            if (s2 == 'B') { // DOWN — lane down
                                if (gs.player_lane < LANE_LOW) gs.player_lane++;
                            }
                            if (s2 == 'C') { // RIGHT — nudge forward
                                if (gs.player_col < gs.field_w - 2) gs.player_col++;
                            }
                            if (s2 == 'D') { // LEFT — nudge back
                                if (gs.player_col > 3) gs.player_col--;
                            }
                        }
                    }
                }
            }
            if (quit) break;

            // --- Scroll waveform ---
            scroll_waveform(&gs);

            // Player col effectively moves left as wave scrolls
            // If player is standing still, they drift left by 1 each frame
            // Compensate: player_col stays fixed (wave moves under them)
            // But if player drifts off left edge, lose a life
            if (gs.player_col < 1) {
                gs.lives--;
                gs.player_col = PLAYER_COL_OFFSET;
                gs.invuln_frames = 20;
                if (gs.lives <= 0) game_over = true;
            }

            // --- Invulnerability countdown ---
            if (gs.invuln_frames > 0) gs.invuln_frames--;

            // --- Scoring ---
            frame++;
            if (frame % 2 == 0) gs.score++;
            // Bonus for riding the signal
            if (on_signal(&gs)) {
                if (frame % 3 == 0) gs.score++;
            }

            // --- Speed up ---
            if (gs.score > 0 && gs.score % 100 == 0 && gs.tick_ms > TICK_MS_MIN) {
                gs.tick_ms -= 3;
            }

            // --- Draw ---
            tx_fifo_wait_drain();
            draw_frame(&gs);

            // --- Collision ---
            if (gs.invuln_frames == 0 && check_hazard(&gs)) {
                gs.lives--;
                gs.invuln_frames = 20;
                // Flash hit
                ui_term_cursor_position(gs.lane_row[gs.player_lane], gs.player_col);
                printf("%s*%s", red, rst);
                tx_fifo_wait_drain();
                busy_wait_ms(200);
                if (gs.lives <= 0) {
                    game_over = true;
                }
            }

            // --- Timing ---
            uint32_t elapsed = (time_us_32() - tick_start) / 1000;
            if ((int)elapsed < gs.tick_ms) {
                busy_wait_ms(gs.tick_ms - (int)elapsed);
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
            printf("%s|    SIGNAL LOST !!!       |%s", red, rst);
            ui_term_cursor_position(mr, mc);
            printf("%s|  Waveform flatlined.     |%s", red, rst);
            ui_term_cursor_position(mr + 1, mc);
            printf("%s|  Score: %s%-6d%s           |%s", red, ylw, gs.score, red, rst);
            ui_term_cursor_position(mr + 2, mc);
            printf("%s+==========================+%s", red, rst);
            ui_term_cursor_position(mr + 4, (gs.field_w - 28) / 2);
            printf("SPACE = ride again   q = quit");
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
