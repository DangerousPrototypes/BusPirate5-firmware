// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file cryptocrack.c
 * @brief Crypto Crack — timed cipher/decode speed-drill game for Bus Pirate.
 *
 * Challenge types:
 *   - Caesar shift decode (shift 1-25, decode a short word)
 *   - XOR decode (single-byte key, decode hex ciphertext to ASCII)
 *   - Hex → ASCII  (convert hex pairs to letters)
 *   - ASCII → Hex  (convert a word to hex pairs)
 *   - Binary → Decimal (convert an 8-bit binary string to decimal)
 *   - Decimal → Binary (convert a decimal 0-255 to 8-bit binary)
 *
 * Streak multiplier rewards consecutive fast answers.
 * Timed rounds with score tracking and hi-score leaderboard.
 *
 * Controls: type answer + ENTER, q = quit.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
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
#include "cryptocrack.h"

static const char* const usage[] = {
    "crack",
    "Launch Crypto Crack:%s crack",
    "",
    "Timed cipher and decode speed drill!",
    "Type answer + ENTER.  q = quit.",
};

const bp_command_def_t cryptocrack_def = {
    .name = "crack",
    .description = T_HELP_CRYPTOCRACK,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define ROUND_TIME_SEC  60      // seconds per round
#define MAX_INPUT       40      // max answer buffer
#define CHALLENGE_TYPES 6

// ---------------------------------------------------------------------------
// Game state — stack-allocated, passed as pointer to all helpers
// ---------------------------------------------------------------------------
typedef struct {
    // ANSI color shortcuts (set once per round based on terminal_ansi_color)
    const char* grn;
    const char* red;
    const char* ylw;
    const char* cyn;
    const char* wht;
    const char* dim;
    const char* rev;
    const char* rst_s;
    // Scores
    int score;
    int hi_score;
    int streak;
    int best_streak;
    int solved;
    // Current challenge
    int challenge_type;
    char question[80];
    char answer[MAX_INPUT + 1];
    // Terminal dimensions
    int field_w;
    int field_h;
} crypto_state_t;

// ---------------------------------------------------------------------------
// ANSI helpers
// ---------------------------------------------------------------------------
static const char* R(crypto_state_t* gs) { return system_config.terminal_ansi_color ? gs->rst_s : ""; }
static const char* C(const char* c) { return system_config.terminal_ansi_color ? c : ""; }

// Word banks for Caesar / hex-ascii challenges
static const char* const wordbank[] = {
    "UART", "SPI", "I2C", "JTAG", "MOSI", "MISO",
    "SCLK", "GPIO", "NAND", "FLASH", "RESET", "CLOCK",
    "PROBE", "DEBUG", "LOGIC", "BAUD", "PARITY", "BIT",
    "BYTE", "WORD", "DATA", "BUS", "CHIP", "WIRE",
    "FIFO", "LATCH", "PULL", "PUSH", "EDGE", "SYNC",
    "ACK", "NAK", "CRC", "DMA", "IRQ", "PWM",
    "ADC", "DAC", "OTP", "RAM", "ROM", "LED",
};
#define WORDBANK_SIZE (sizeof(wordbank) / sizeof(wordbank[0]))



// ---------------------------------------------------------------------------
// Challenge generators
// ---------------------------------------------------------------------------

// Fill question[] and answer[] for a Caesar shift challenge
static void gen_caesar(crypto_state_t* gs) {
    const char* word = wordbank[game_rng_next() % WORDBANK_SIZE];
    int shift = (int)(game_rng_next() % 25) + 1; // 1-25
    int len = (int)strlen(word);

    char cipher[20];
    for (int i = 0; i < len && i < 19; i++) {
        char c = word[i];
        if (c >= 'A' && c <= 'Z') {
            cipher[i] = (char)('A' + ((c - 'A' + shift) % 26));
        } else {
            cipher[i] = c;
        }
    }
    cipher[len] = '\0';

    snprintf(gs->question, sizeof(gs->question),
             "Caesar shift=%d:  %s  ->  plaintext?", shift, cipher);
    snprintf(gs->answer, sizeof(gs->answer), "%s", word);
}

// XOR decode: single-byte key, show hex ciphertext, answer is ASCII
static void gen_xor(crypto_state_t* gs) {
    const char* word = wordbank[game_rng_next() % WORDBANK_SIZE];
    uint8_t key = (uint8_t)((game_rng_next() % 0xFE) + 1); // 1-254
    int len = (int)strlen(word);

    char hex[60];
    int pos = 0;
    for (int i = 0; i < len && pos < 56; i++) {
        uint8_t enc = (uint8_t)word[i] ^ key;
        pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", enc);
        if (i < len - 1) hex[pos++] = ' ';
    }
    hex[pos] = '\0';

    snprintf(gs->question, sizeof(gs->question),
             "XOR key=0x%02X:  %s  ->  ASCII?", key, hex);
    snprintf(gs->answer, sizeof(gs->answer), "%s", word);
}

// Hex → ASCII: show hex pairs, answer is the word
static void gen_hex_to_ascii(crypto_state_t* gs) {
    const char* word = wordbank[game_rng_next() % WORDBANK_SIZE];
    int len = (int)strlen(word);

    char hex[60];
    int pos = 0;
    for (int i = 0; i < len && pos < 56; i++) {
        pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", (uint8_t)word[i]);
        if (i < len - 1) hex[pos++] = ' ';
    }
    hex[pos] = '\0';

    snprintf(gs->question, sizeof(gs->question), "Hex -> ASCII:  %s  ->  ?", hex);
    snprintf(gs->answer, sizeof(gs->answer), "%s", word);
}

// ASCII → Hex: show word, answer is hex pairs (no spaces)
static void gen_ascii_to_hex(crypto_state_t* gs) {
    const char* word = wordbank[game_rng_next() % WORDBANK_SIZE];
    int len = (int)strlen(word);

    snprintf(gs->question, sizeof(gs->question), "ASCII -> Hex:  %s  ->  ?", word);

    int pos = 0;
    for (int i = 0; i < len && pos < MAX_INPUT - 2; i++) {
        pos += snprintf(gs->answer + pos, sizeof(gs->answer) - pos, "%02X", (uint8_t)word[i]);
    }
    gs->answer[pos] = '\0';
}

// Binary → Decimal: show 8-bit binary, answer is decimal
static void gen_bin_to_dec(crypto_state_t* gs) {
    uint32_t val = game_rng_next() % 256;

    char bits[9];
    for (int i = 7; i >= 0; i--) {
        bits[7 - i] = (val & (1u << i)) ? '1' : '0';
    }
    bits[8] = '\0';

    snprintf(gs->question, sizeof(gs->question), "Binary -> Decimal:  %s  ->  ?", bits);
    snprintf(gs->answer, sizeof(gs->answer), "%d", (int)val);
}

// Decimal → Binary: show decimal, answer is 8-bit binary string
static void gen_dec_to_bin(crypto_state_t* gs) {
    uint32_t val = game_rng_next() % 256;

    snprintf(gs->question, sizeof(gs->question), "Decimal -> Binary:  %d  ->  ?", (int)val);

    char bits[9];
    for (int i = 7; i >= 0; i--) {
        bits[7 - i] = (val & (1u << i)) ? '1' : '0';
    }
    bits[8] = '\0';
    snprintf(gs->answer, sizeof(gs->answer), "%s", bits);
}

typedef void (*gen_fn)(crypto_state_t*);
static const gen_fn generators[CHALLENGE_TYPES] = {
    gen_caesar,
    gen_xor,
    gen_hex_to_ascii,
    gen_ascii_to_hex,
    gen_bin_to_dec,
    gen_dec_to_bin,
};

static const char* const type_names[CHALLENGE_TYPES] = {
    "CAESAR", "XOR", "HEX>ASC", "ASC>HEX", "BIN>DEC", "DEC>BIN",
};

static void generate_challenge(crypto_state_t* gs) {
    gs->challenge_type = (int)(game_rng_next() % CHALLENGE_TYPES);
    generators[gs->challenge_type](gs);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void draw_header(int sr, int sc, crypto_state_t* gs) {
    ui_term_cursor_position(sr, sc);
    printf("%s%s  C R Y P T O   C R A C K  %s", C(gs->rev), C(gs->wht), R(gs));
}

static void draw_status(int sr, int sc, int time_left, crypto_state_t* gs) {
    ui_term_cursor_position(sr, sc);
    printf("%sScore:%s %-6d %sStreak:%s x%-3d %sBest:%s x%-3d %sSolved:%s %-3d %sTime:%s %02d%s\x1b[K",
           C(gs->ylw), R(gs), gs->score,
           C(gs->cyn), R(gs), gs->streak,
           C(gs->grn), R(gs), gs->best_streak,
           C(gs->wht), R(gs), gs->solved,
           time_left <= 10 ? C(gs->red) : C(gs->grn), R(gs), time_left, R(gs));
}

static void draw_challenge(int sr, int sc, crypto_state_t* gs) {
    // Type tag
    ui_term_cursor_position(sr, sc);
    printf("%s[%s]%s\x1b[K", C(gs->dim), type_names[gs->challenge_type], R(gs));

    // Question
    ui_term_cursor_position(sr + 2, sc);
    printf("%s%s%s\x1b[K", C(gs->wht), gs->question, R(gs));

    // Answer prompt
    ui_term_cursor_position(sr + 4, sc);
    printf("%s>%s \x1b[K", C(gs->cyn), R(gs));
}

static void draw_result(int sr, int sc, bool correct, const char* correct_answer, crypto_state_t* gs) {
    ui_term_cursor_position(sr, sc);
    if (correct) {
        printf("%s  CORRECT!  +%d pts  (streak x%d)  %s\x1b[K",
               C(gs->grn), 10 * gs->streak, gs->streak, R(gs));
    } else {
        printf("%s  WRONG!  Answer: %s  %s\x1b[K",
               C(gs->red), correct_answer, R(gs));
    }
}

static void draw_scoreboard(int sr, int sc, crypto_state_t* gs) {
    ui_term_cursor_position(sr, sc);
    printf("%s%s+------------------------------+%s", C(gs->rev), C(gs->cyn), R(gs));
    ui_term_cursor_position(sr + 1, sc);
    printf("%s%s|     R O U N D   O V E R      |%s", C(gs->rev), C(gs->cyn), R(gs));
    ui_term_cursor_position(sr + 2, sc);
    printf("%s%s+------------------------------+%s", C(gs->rev), C(gs->cyn), R(gs));
    ui_term_cursor_position(sr + 4, sc);
    printf("  Final Score:  %s%d%s", C(gs->ylw), gs->score, R(gs));
    ui_term_cursor_position(sr + 5, sc);
    printf("  Challenges:   %s%d%s", C(gs->wht), gs->solved, R(gs));
    ui_term_cursor_position(sr + 6, sc);
    printf("  Best Streak:  %s%d%s", C(gs->grn), gs->best_streak, R(gs));
    ui_term_cursor_position(sr + 7, sc);
    printf("  Hi-Score:     %s%d%s", C(gs->red), gs->hi_score, R(gs));
    ui_term_cursor_position(sr + 9, sc);
    printf("  %sSPACE%s = play again   %sq%s = quit", C(gs->wht), R(gs), C(gs->wht), R(gs));
}

static void draw_help_bar(int row, crypto_state_t* gs) {
    ui_term_cursor_position(row, 1);
    printf("%s Type answer + ENTER | q=quit %s\x1b[K", C(gs->dim), R(gs));
}

// ---------------------------------------------------------------------------
// Input helper: blocking line-read with echo, supports backspace.
// Periodically refreshes the countdown timer on the status row.
// Returns length, or -1 if quit, -2 if timeout
// ---------------------------------------------------------------------------
static int read_line(char* buf, int maxlen, int echo_row, int echo_col,
                     absolute_time_t deadline, int status_row, int status_col,
                     crypto_state_t* gs) {
    int len = 0;
    buf[0] = '\0';
    int last_sec = -1;

    // Position cursor
    ui_term_cursor_position(echo_row, echo_col);
    printf("%s", ui_term_cursor_show());

    while (len < maxlen) {
        if (time_reached(deadline)) {
            printf("%s", ui_term_cursor_hide());
            return -2; // timeout
        }

        // Update timer once per second without disturbing typing cursor
        int64_t rem_us = absolute_time_diff_us(get_absolute_time(), deadline);
        int sec = (int)(rem_us / 1000000);
        if (sec < 0) sec = 0;
        if (sec != last_sec) {
            last_sec = sec;
            printf("\x1b[s"); // save cursor
            draw_status(status_row, status_col, sec, gs);
            printf("\x1b[u"); // restore cursor
        }

        char c;
        if (!rx_fifo_try_get(&c)) {
            busy_wait_ms(5);
            continue;
        }
        if (c == 'q' && len == 0) {
            printf("%s", ui_term_cursor_hide());
            return -1; // quit
        }
        if (c == '\r' || c == '\n') {
            break;
        }
        if (c == 0x7f || c == '\b') {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                printf("\b \b");
            }
            continue;
        }
        // Accept printable characters
        if (c >= 0x20 && c < 0x7f) {
            buf[len++] = c;
            buf[len] = '\0';
            // Echo uppercase
            printf("%c", toupper((unsigned char)c));
        }
    }

    printf("%s", ui_term_cursor_hide());
    return len;
}

// Case-insensitive compare
static bool answer_matches(const char* input, const char* expected) {
    int i = 0;
    while (input[i] && expected[i]) {
        if (toupper((unsigned char)input[i]) != toupper((unsigned char)expected[i])) return false;
        i++;
    }
    return input[i] == '\0' && expected[i] == '\0';
}

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void cryptocrack_handler(struct command_result* res) {
    if (bp_cmd_help_check(&cryptocrack_def, res->help_flag)) return;

    crypto_state_t gs = {0};

    // ANSI color shortcuts
    if (system_config.terminal_ansi_color) {
        gs.grn   = "\x1b[32m";
        gs.red   = "\x1b[31m";
        gs.ylw   = "\x1b[33m";
        gs.cyn   = "\x1b[36m";
        gs.wht   = "\x1b[1;37m";
        gs.dim   = "\x1b[2m";
        gs.rev   = "\x1b[7m";
        gs.rst_s = "\x1b[0m";
    } else {
        gs.grn = gs.red = gs.ylw = gs.cyn = gs.wht = gs.dim = gs.rev = gs.rst_s = "";
    }

    gs.field_w = (int)system_config.terminal_ansi_columns;
    gs.field_h = (int)system_config.terminal_ansi_rows;
    if (gs.field_w < 40 || gs.field_h < 16) {
        printf("Terminal too small (need at least 40x16)\r\n");
        return;
    }

    game_rng_seed();
    game_screen_enter(0);

    // Layout: centered
    int content_w = 50;
    int sc = (gs.field_w - content_w) / 2;
    if (sc < 2) sc = 2;
    int sr = 2;

    bool quit = false;

    while (!quit) {
        // --- New round ---
        gs.score = 0;
        gs.streak = 0;
        gs.best_streak = 0;
        gs.solved = 0;

        printf("\x1b[2J"); // clear screen
        draw_header(sr, sc, &gs);
        draw_help_bar(gs.field_h, &gs);

        // Countdown
        for (int i = 3; i >= 1; i--) {
            ui_term_cursor_position(sr + 4, sc + 10);
            printf("%s%s%d...%s\x1b[K", C(gs.wht), C(gs.rev), i, R(&gs));
            tx_fifo_wait_drain();
            busy_wait_ms(800);
        }
        ui_term_cursor_position(sr + 4, sc + 10);
        printf("%sGO!%s\x1b[K", C(gs.grn), R(&gs));
        tx_fifo_wait_drain();
        busy_wait_ms(400);

        absolute_time_t round_end = make_timeout_time_ms(ROUND_TIME_SEC * 1000);

        while (!quit) {
            // Time left
            int64_t remaining_us = absolute_time_diff_us(get_absolute_time(), round_end);
            if (remaining_us <= 0) break; // round over
            int time_left = (int)(remaining_us / 1000000);
            if (time_left < 0) time_left = 0;

            // Generate and display challenge
            generate_challenge(&gs);

            draw_status(sr + 2, sc, time_left, &gs);
            draw_challenge(sr + 5, sc, &gs);

            // Clear previous result area
            ui_term_cursor_position(sr + 10, sc);
            printf("\x1b[K");

            // Read answer
            char input[MAX_INPUT + 1];
            int echo_col = sc + 2;
            int result = read_line(input, MAX_INPUT, sr + 9, echo_col, round_end, sr + 2, sc, &gs);

            if (result == -1) { // quit
                quit = true;
                break;
            }

            remaining_us = absolute_time_diff_us(get_absolute_time(), round_end);
            if (remaining_us <= 0 || result == -2) break; // timeout

            // Check answer
            bool correct = answer_matches(input, gs.answer);

            if (correct) {
                gs.streak++;
                if (gs.streak > gs.best_streak) gs.best_streak = gs.streak;
                int points = 10 * gs.streak; // streak multiplier
                gs.score += points;
                gs.solved++;
            } else {
                gs.streak = 0;
            }

            time_left = (int)(absolute_time_diff_us(get_absolute_time(), round_end) / 1000000);
            if (time_left < 0) time_left = 0;
            draw_status(sr + 2, sc, time_left, &gs);
            draw_result(sr + 11, sc, correct, gs.answer, &gs);
            tx_fifo_wait_drain();
            busy_wait_ms(correct ? 500 : 1200);
        }

        // --- Round over ---
        if (gs.score > gs.hi_score) gs.hi_score = gs.score;

        if (!quit) {
            printf("\x1b[2J");
            draw_scoreboard(sr + 2, sc, &gs);
            tx_fifo_wait_drain();

            // Wait for input
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
