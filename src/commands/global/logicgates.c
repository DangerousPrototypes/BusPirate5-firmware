// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file logicgates.c
 * @brief Logic Gates — truth-table puzzle game for Bus Pirate.
 *
 * Given a truth table, identify which logic expression produces the output.
 * Levels progress from single gates through compound expressions.
 *
 * Level tiers:
 *   1-3:  Single 2-input gate  (AND, OR, XOR, NAND, NOR, XNOR)
 *   4-6:  NOT + gate            (NOT(A AND B), A OR (NOT B), …)
 *   7-9:  Two-gate chain        ((A AND B) XOR C, …) — 3 inputs
 *   10+:  Complex expressions   (3-input with nested ops)
 *
 * Pick the correct expression from 4 choices. Streak multiplier for speed.
 * Timer per puzzle; wrong answer or timeout ends the game.
 *
 * Controls: 1-4 = pick answer, q = quit.
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
#include "logicgates.h"

static const char* const usage[] = {
    "gates",
    "Launch Logic Gates:%s gates",
    "",
    "Identify the logic expression from a truth table!",
    "1-4=pick answer  q=quit",
};

const bp_command_def_t logicgates_def = {
    .name = "gates",
    .description = T_HELP_LOGICGATES,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// ANSI helpers
// ---------------------------------------------------------------------------
static const char* CC(const char* c) { return system_config.terminal_ansi_color ? c : ""; }
static const char* RR(void) { return system_config.terminal_ansi_color ? "\x1b[0m" : ""; }

#define GRN  "\x1b[32m"
#define RED  "\x1b[31m"
#define YLW  "\x1b[33m"
#define CYN  "\x1b[36m"
#define WHT  "\x1b[1;37m"
#define DIM  "\x1b[2m"
#define REV  "\x1b[7m"

// ---------------------------------------------------------------------------
// Gate definitions  (all operate on bool inputs → bool output)
// ---------------------------------------------------------------------------

// 2-input gate functions
typedef bool (*gate2_fn)(bool a, bool b);

static bool g_and(bool a, bool b)  { return a && b; }
static bool g_or(bool a, bool b)   { return a || b; }
static bool g_xor(bool a, bool b)  { return a != b; }
static bool g_nand(bool a, bool b) { return !(a && b); }
static bool g_nor(bool a, bool b)  { return !(a || b); }
static bool g_xnor(bool a, bool b) { return a == b; }

// Expression types — encodes how to evaluate and display
typedef enum {
    // 2-input single gates
    EXPR_AND, EXPR_OR, EXPR_XOR, EXPR_NAND, EXPR_NOR, EXPR_XNOR,
    // NOT + 2-input
    EXPR_NOT_A_AND_B,   // (NOT A) AND B
    EXPR_A_AND_NOT_B,   // A AND (NOT B)
    EXPR_NOT_A_OR_B,    // (NOT A) OR B
    EXPR_A_OR_NOT_B,    // A OR (NOT B)
    EXPR_NOT_A_XOR_B,   // (NOT A) XOR B
    EXPR_A_XOR_NOT_B,   // A XOR (NOT B)
    // 3-input expressions
    EXPR_AB_AND_C,      // (A AND B) AND C
    EXPR_AB_OR_C,       // (A AND B) OR C
    EXPR_AB_XOR_C,      // (A AND B) XOR C
    EXPR_AoB_AND_C,     // (A OR B) AND C
    EXPR_AoB_OR_C,      // (A OR B) OR C — trivial but fills options
    EXPR_AoB_XOR_C,     // (A OR B) XOR C
    EXPR_AxB_AND_C,     // (A XOR B) AND C
    EXPR_AxB_OR_C,      // (A XOR B) OR C
    EXPR_COUNT
} expr_type_t;

// String labels for each expression
static const char* const expr_labels[] = {
    [EXPR_AND]          = "A AND B",
    [EXPR_OR]           = "A OR B",
    [EXPR_XOR]          = "A XOR B",
    [EXPR_NAND]         = "A NAND B",
    [EXPR_NOR]          = "A NOR B",
    [EXPR_XNOR]         = "A XNOR B",
    [EXPR_NOT_A_AND_B]  = "(NOT A) AND B",
    [EXPR_A_AND_NOT_B]  = "A AND (NOT B)",
    [EXPR_NOT_A_OR_B]   = "(NOT A) OR B",
    [EXPR_A_OR_NOT_B]   = "A OR (NOT B)",
    [EXPR_NOT_A_XOR_B]  = "(NOT A) XOR B",
    [EXPR_A_XOR_NOT_B]  = "A XOR (NOT B)",
    [EXPR_AB_AND_C]     = "(A AND B) AND C",
    [EXPR_AB_OR_C]      = "(A AND B) OR C",
    [EXPR_AB_XOR_C]     = "(A AND B) XOR C",
    [EXPR_AoB_AND_C]    = "(A OR B) AND C",
    [EXPR_AoB_OR_C]     = "(A OR B) OR C",
    [EXPR_AoB_XOR_C]    = "(A OR B) XOR C",
    [EXPR_AxB_AND_C]    = "(A XOR B) AND C",
    [EXPR_AxB_OR_C]     = "(A XOR B) OR C",
};

// How many inputs each expression needs
static int expr_inputs(expr_type_t e) {
    return (e >= EXPR_AB_AND_C) ? 3 : 2;
}

// Evaluate an expression given inputs
static bool expr_eval(expr_type_t e, bool a, bool b, bool c) {
    switch (e) {
        case EXPR_AND:          return g_and(a, b);
        case EXPR_OR:           return g_or(a, b);
        case EXPR_XOR:          return g_xor(a, b);
        case EXPR_NAND:         return g_nand(a, b);
        case EXPR_NOR:          return g_nor(a, b);
        case EXPR_XNOR:         return g_xnor(a, b);
        case EXPR_NOT_A_AND_B:  return g_and(!a, b);
        case EXPR_A_AND_NOT_B:  return g_and(a, !b);
        case EXPR_NOT_A_OR_B:   return g_or(!a, b);
        case EXPR_A_OR_NOT_B:   return g_or(a, !b);
        case EXPR_NOT_A_XOR_B:  return g_xor(!a, b);
        case EXPR_A_XOR_NOT_B:  return g_xor(a, !b);
        case EXPR_AB_AND_C:     return g_and(g_and(a, b), c);
        case EXPR_AB_OR_C:      return g_or(g_and(a, b), c);
        case EXPR_AB_XOR_C:     return g_xor(g_and(a, b), c);
        case EXPR_AoB_AND_C:    return g_and(g_or(a, b), c);
        case EXPR_AoB_OR_C:     return g_or(g_or(a, b), c);
        case EXPR_AoB_XOR_C:    return g_xor(g_or(a, b), c);
        case EXPR_AxB_AND_C:    return g_and(g_xor(a, b), c);
        case EXPR_AxB_OR_C:     return g_or(g_xor(a, b), c);
        default:                return false;
    }
}

// Compute 4-bit (2-input) or 8-bit (3-input) output signature for an expression
static uint8_t expr_signature(expr_type_t e) {
    int n = expr_inputs(e);
    uint8_t sig = 0;
    int rows = (n == 2) ? 4 : 8;
    for (int i = 0; i < rows; i++) {
        bool a = (i >> (n - 1)) & 1;
        bool b = (i >> (n - 2)) & 1;
        bool c = (n == 3) ? (i & 1) : false;
        if (expr_eval(e, a, b, c)) sig |= (1 << i);
    }
    return sig;
}

// ---------------------------------------------------------------------------
// Puzzle generation
// ---------------------------------------------------------------------------

// Game state
typedef struct {
    int score;
    int hi_score;
    int level;
    int streak;
    int best_streak;
    int solved;
    expr_type_t correct_expr;
    int correct_choice;
    expr_type_t choices[4];
    int num_inputs;
    uint8_t truth_sig;
} logic_state_t;

// Difficulty tiers by level
static void pick_expr_pool(int lvl, int* pool, int* pool_size) {
    // pool[] filled with valid expr_type_t indices
    // Tier 1 (level 1-3): simple 2-input gates
    // Tier 2 (level 4-6): add NOT combos
    // Tier 3 (level 7+): add 3-input combos
    int n = 0;
    // Always include basic gates
    pool[n++] = EXPR_AND;
    pool[n++] = EXPR_OR;
    pool[n++] = EXPR_XOR;
    pool[n++] = EXPR_NAND;
    pool[n++] = EXPR_NOR;
    pool[n++] = EXPR_XNOR;
    if (lvl >= 4) {
        pool[n++] = EXPR_NOT_A_AND_B;
        pool[n++] = EXPR_A_AND_NOT_B;
        pool[n++] = EXPR_NOT_A_OR_B;
        pool[n++] = EXPR_A_OR_NOT_B;
        pool[n++] = EXPR_NOT_A_XOR_B;
        pool[n++] = EXPR_A_XOR_NOT_B;
    }
    if (lvl >= 7) {
        pool[n++] = EXPR_AB_AND_C;
        pool[n++] = EXPR_AB_OR_C;
        pool[n++] = EXPR_AB_XOR_C;
        pool[n++] = EXPR_AoB_AND_C;
        pool[n++] = EXPR_AoB_OR_C;
        pool[n++] = EXPR_AoB_XOR_C;
        pool[n++] = EXPR_AxB_AND_C;
        pool[n++] = EXPR_AxB_OR_C;
    }
    *pool_size = n;
}

static void generate_puzzle(logic_state_t* gs, int lvl) {
    int pool[EXPR_COUNT];
    int pool_size;
    pick_expr_pool(lvl, pool, &pool_size);

    // Pick correct answer
    gs->correct_expr = (expr_type_t)pool[game_rng_next() % pool_size];
    gs->num_inputs = expr_inputs(gs->correct_expr);
    gs->truth_sig = expr_signature(gs->correct_expr);

    // Place correct answer in a random slot
    gs->correct_choice = (int)(game_rng_next() % 4);

    // Fill 4 choices: one correct, three wrong (different signatures)
    gs->choices[gs->correct_choice] = gs->correct_expr;

    for (int slot = 0; slot < 4; slot++) {
        if (slot == gs->correct_choice) continue;

        // Pick a wrong answer with a different signature and same input count
        int attempts = 0;
        while (attempts < 200) {
            expr_type_t candidate = (expr_type_t)pool[game_rng_next() % pool_size];
            if (expr_inputs(candidate) != gs->num_inputs) { attempts++; continue; }
            if (expr_signature(candidate) == gs->truth_sig) { attempts++; continue; }
            // Check not duplicate of already-placed choices
            bool dup = false;
            for (int k = 0; k < slot; k++) {
                if (k == gs->correct_choice) continue; // skip, checked above
                if (gs->choices[k] == candidate) { dup = true; break; }
            }
            // Also check against the correct choice
            if (candidate == gs->correct_expr) { attempts++; continue; }
            if (!dup) {
                gs->choices[slot] = candidate;
                break;
            }
            attempts++;
        }
        // If we exhausted attempts, just pick something different
        if (attempts >= 200) {
            for (int e = 0; e < EXPR_COUNT; e++) {
                if (e != (int)gs->correct_expr &&
                    expr_inputs((expr_type_t)e) == gs->num_inputs &&
                    expr_signature((expr_type_t)e) != gs->truth_sig) {
                    gs->choices[slot] = (expr_type_t)e;
                    break;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void draw_header(int sr, int sc) {
    ui_term_cursor_position(sr, sc);
    printf("%s%s  L O G I C   G A T E S  %s", CC(REV), CC(WHT), RR());
}

static void draw_status(logic_state_t* gs, int sr, int sc, int time_left) {
    ui_term_cursor_position(sr, sc);
    printf("%sLv:%s %-2d  %sScore:%s %-5d  %sStreak:%s x%-2d  "
           "%sBest:%s x%-2d  %sSolved:%s %-3d  %sTime:%s %2d%s\x1b[K",
           CC(CYN), RR(), gs->level,
           CC(YLW), RR(), gs->score,
           CC(GRN), RR(), gs->streak,
           CC(WHT), RR(), gs->best_streak,
           CC(WHT), RR(), gs->solved,
           time_left <= 5 ? CC(RED) : CC(GRN), RR(), time_left, RR());
}

static void draw_truth_table(logic_state_t* gs, int sr, int sc) {
    int rows = (gs->num_inputs == 2) ? 4 : 8;

    // Header
    ui_term_cursor_position(sr, sc);
    if (gs->num_inputs == 2) {
        printf("%s A  B %s|%s OUT %s", CC(WHT), CC(DIM), CC(YLW), RR());
    } else {
        printf("%s A  B  C %s|%s OUT %s", CC(WHT), CC(DIM), CC(YLW), RR());
    }
    ui_term_cursor_position(sr + 1, sc);
    if (gs->num_inputs == 2) {
        printf("%s--------+-----%s", CC(DIM), RR());
    } else {
        printf("%s-----------+-----%s", CC(DIM), RR());
    }

    for (int i = 0; i < rows; i++) {
        bool a = (i >> (gs->num_inputs - 1)) & 1;
        bool b = (i >> (gs->num_inputs - 2)) & 1;
        bool c_val = (gs->num_inputs == 3) ? (i & 1) : false;
        bool out = (gs->truth_sig >> i) & 1;

        ui_term_cursor_position(sr + 2 + i, sc);
        if (gs->num_inputs == 2) {
            printf(" %s%d  %d %s|%s  %s%d%s  ",
                   CC(WHT), a, b, CC(DIM), RR(),
                   out ? CC(GRN) : CC(RED), out, RR());
        } else {
            printf(" %s%d  %d  %d %s|%s  %s%d%s  ",
                   CC(WHT), a, b, c_val, CC(DIM), RR(),
                   out ? CC(GRN) : CC(RED), out, RR());
        }
    }
}

static void draw_choices(logic_state_t* gs, int sr, int sc) {
    ui_term_cursor_position(sr, sc);
    printf("%sWhich expression produces this output?%s", CC(WHT), RR());

    for (int i = 0; i < 4; i++) {
        ui_term_cursor_position(sr + 2 + i, sc);
        printf("  %s%d)%s  %s\x1b[K", CC(CYN), i + 1, RR(), expr_labels[gs->choices[i]]);
    }
}

static void draw_result(logic_state_t* gs, int sr, int sc, bool correct) {
    ui_term_cursor_position(sr, sc);
    if (correct) {
        printf("%s  \xe2\x9c\x93 CORRECT!  +%d pts  (streak x%d)  %s\x1b[K",
               CC(GRN), 10 * gs->streak, gs->streak, RR());
    } else {
        printf("%s  \xe2\x9c\x97 WRONG!  Answer: %s  %s\x1b[K",
               CC(RED), expr_labels[gs->correct_expr], RR());
    }
}

static void draw_schematic(logic_state_t* gs, int sr, int sc, expr_type_t e, bool reveal) {
    // ASCII gate schematic: shows [???] until answered, then the real expression
    ui_term_cursor_position(sr, sc);
    const char* label = reveal ? expr_labels[e] : "???";
    const char* lclr = reveal ? CC(GRN) : CC(YLW);

    if (gs->num_inputs == 2) {
        printf("%sA -%s+--[%s%s%s]--%s> Q%s",
               CC(WHT), CC(DIM), lclr, label, CC(DIM), CC(GRN), RR());
        ui_term_cursor_position(sr + 1, sc);
        printf("%sB -%s+%s", CC(WHT), CC(DIM), RR());
    } else {
        printf("%sA -%s+%s", CC(WHT), CC(DIM), RR());
        ui_term_cursor_position(sr + 1, sc);
        printf("%sB -%s+-[%s%s%s]--%s> Q%s",
               CC(WHT), CC(DIM), lclr, label, CC(DIM), CC(GRN), RR());
        ui_term_cursor_position(sr + 2, sc);
        printf("%sC -%s+%s", CC(WHT), CC(DIM), RR());
    }
}

static void draw_help_bar(int row) {
    ui_term_cursor_position(row, 1);
    printf("%s 1-4=pick answer | q=quit %s\x1b[K", CC(DIM), RR());
}

static void draw_scoreboard(logic_state_t* gs, int sr, int sc) {
    ui_term_cursor_position(sr, sc);
    printf("%s%s+----------------------------+%s", CC(REV), CC(CYN), RR());
    ui_term_cursor_position(sr + 1, sc);
    printf("%s%s|   G A M E   O V E R        |%s", CC(REV), CC(CYN), RR());
    ui_term_cursor_position(sr + 2, sc);
    printf("%s%s+----------------------------+%s", CC(REV), CC(CYN), RR());
    ui_term_cursor_position(sr + 4, sc);
    printf("  Final Score:  %s%d%s", CC(YLW), gs->score, RR());
    ui_term_cursor_position(sr + 5, sc);
    printf("  Solved:       %s%d%s", CC(WHT), gs->solved, RR());
    ui_term_cursor_position(sr + 6, sc);
    printf("  Level:        %s%d%s", CC(CYN), gs->level, RR());
    ui_term_cursor_position(sr + 7, sc);
    printf("  Best Streak:  %s%d%s", CC(GRN), gs->best_streak, RR());
    ui_term_cursor_position(sr + 8, sc);
    printf("  Hi-Score:     %s%d%s", CC(RED), gs->hi_score, RR());
    ui_term_cursor_position(sr + 10, sc);
    printf("  %sSPACE%s = play again   %sq%s = quit", CC(WHT), RR(), CC(WHT), RR());
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void logicgates_handler(struct command_result* res) {
    if (bp_cmd_help_check(&logicgates_def, res->help_flag)) return;

    int fw = (int)system_config.terminal_ansi_columns;
    int fh = (int)system_config.terminal_ansi_rows;
    if (fw < 50 || fh < 20) {
        printf("Terminal too small (need at least 50x20)\r\n");
        return;
    }

    game_rng_seed();
    game_screen_enter(0);

    logic_state_t gs = {0};

    int sc = 4;        // left margin
    int sr = 2;        // top margin
    // Puzzle time limit: 20s for easy, scaling down
    int time_per_puzzle;

    bool quit = false;

    while (!quit) {
        // --- New game ---
        gs.score = 0;
        gs.streak = 0;
        gs.best_streak = 0;
        gs.solved = 0;
        gs.level = 1;

        printf("\x1b[2J");
        draw_header(sr, sc);
        draw_help_bar(fh);

        // Countdown
        for (int i = 3; i >= 1; i--) {
            ui_term_cursor_position(sr + 4, sc + 8);
            printf("%s%s%d...%s\x1b[K", CC(WHT), CC(REV), i, RR());
            tx_fifo_wait_drain();
            busy_wait_ms(700);
        }
        ui_term_cursor_position(sr + 4, sc + 8);
        printf("%sGO!%s\x1b[K", CC(GRN), RR());
        tx_fifo_wait_drain();
        busy_wait_ms(400);

        bool game_active = true;

        while (game_active && !quit) {
            // Time per puzzle: 20s at level 1, down to 8s at level 10+
            time_per_puzzle = 20 - gs.level;
            if (time_per_puzzle < 8) time_per_puzzle = 8;

            generate_puzzle(&gs, gs.level);

            // Clear and draw puzzle
            printf("\x1b[2J");
            draw_header(sr, sc);
            draw_help_bar(fh);

            int tt_row = sr + 2;
            draw_truth_table(&gs, tt_row, sc);

            int table_h = (gs.num_inputs == 2) ? 6 : 10;
            int choices_row = tt_row + table_h + 1;
            draw_choices(&gs, choices_row, sc);

            // Schematic on the right side — hidden until answered
            int schem_col = sc + 30;
            draw_schematic(&gs, tt_row + 2, schem_col, gs.correct_expr, false);

            // Status row
            int status_row = sr;

            absolute_time_t deadline = make_timeout_time_ms(time_per_puzzle * 1000);
            int last_sec = -1;
            bool answered = false;

            while (!answered && !quit) {
                // Timer update
                int64_t rem_us = absolute_time_diff_us(get_absolute_time(), deadline);
                int sec = (int)(rem_us / 1000000);
                if (sec < 0) sec = 0;

                if (sec != last_sec) {
                    last_sec = sec;
                    draw_status(&gs, fh - 1, 1, sec);
                    tx_fifo_wait_drain();
                }

                if (rem_us <= 0) {
                    // Time's up — wrong!
                    gs.streak = 0;
                    game_active = false;
                    answered = true;

                    // Reveal the schematic
                    draw_schematic(&gs, tt_row + 2, schem_col, gs.correct_expr, true);

                    int result_row = choices_row + 7;
                    ui_term_cursor_position(result_row, sc);
                    printf("%s  TIME'S UP!  Answer: %s  %s\x1b[K",
                           CC(RED), expr_labels[gs.correct_expr], RR());
                    tx_fifo_wait_drain();
                    busy_wait_ms(1500);
                    break;
                }

                char c;
                if (!rx_fifo_try_get(&c)) {
                    busy_wait_ms(5);
                    continue;
                }

                if (c == 'q' || c == 'Q') { quit = true; break; }

                if (c >= '1' && c <= '4') {
                    int pick = c - '1';
                    answered = true;

                    bool correct = (pick == gs.correct_choice);

                    if (correct) {
                        gs.streak++;
                        if (gs.streak > gs.best_streak) gs.best_streak = gs.streak;
                        int points = 10 * gs.streak;
                        gs.score += points;
                        gs.solved++;

                        // Reveal the schematic with the real expression
                        draw_schematic(&gs, tt_row + 2, schem_col, gs.correct_expr, true);

                        // Level up every 3 solves
                        if (gs.solved % 3 == 0 && gs.level < 12) gs.level++;

                        int result_row = choices_row + 7;
                        draw_result(&gs, result_row, sc, true);
                        tx_fifo_wait_drain();
                        busy_wait_ms(600);
                    } else {
                        gs.streak = 0;
                        game_active = false;

                        // Reveal the schematic with the correct answer
                        draw_schematic(&gs, tt_row + 2, schem_col, gs.correct_expr, true);

                        int result_row = choices_row + 7;
                        draw_result(&gs, result_row, sc, false);
                        tx_fifo_wait_drain();
                        busy_wait_ms(1500);
                    }
                }
            }
        }

        // --- Game Over ---
        if (gs.score > gs.hi_score) gs.hi_score = gs.score;

        if (!quit) {
            printf("\x1b[2J");
            draw_scoreboard(&gs, sr + 2, sc);
            tx_fifo_wait_drain();

            bool waiting = true;
            while (waiting) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; waiting = false; }
                    if (c == ' ') { waiting = false; }
                }
                busy_wait_ms(50);
            }
        }
    }

    game_screen_exit();
}
