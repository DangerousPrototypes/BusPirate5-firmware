// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file hangman.c
 * @brief Hangman — guess the word letter-by-letter for Bus Pirate.
 *
 * EE/protocol themed word list. Type letters to guess, q to quit.
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
#include "hangman.h"

static const char* const usage[] = {
    "hangman",
    "Launch Hangman:%s hangman",
    "",
    "Guess the electronics/protocol themed word before you run out of tries!",
    "a-z=guess  q=quit",
};

const bp_command_def_t hangman_def = {
    .name = "hangman",
    .description = T_HELP_HANGMAN,
    .usage = usage,
    .usage_count = count_of(usage),
};

// ---------------------------------------------------------------------------
// EE / protocol themed word list
// ---------------------------------------------------------------------------
static const char* const words[] = {
    "VOLTAGE",     "GROUND",      "PULLUP",      "PULLDOWN",
    "CLOCK",       "MOSI",        "MISO",        "SERIAL",
    "CHIPSELECT",  "REGISTER",    "PROTOCOL",    "ACKNOWLEDGE",
    "BUSPIRATE",   "FIRMWARE",    "INTERRUPT",   "BITBANG",
    "BAUDRATE",    "EEPROM",      "OPCODE",      "FLASHROM",
    "DATASHEET",   "CHECKSUM",    "MOSFET",      "LOGIC",
    "DUPLEX",      "PARITY",      "BUFFER",      "LATCH",
    "THRESHOLD",   "DEBUGGER",    "OPENDRAIN",   "HANDSHAKE",
    "LOOPBACK",    "SNIFFER",     "TRISTATE",    "ADDRESS",
    "PERIPHERAL",  "STARTBIT",    "STOPBIT",     "ONEWIRE",
    "ENDIAN",      "ARBITRATION", "SCANCHAIN",   "SELFTEST",
    "PINOUT",      "LEVELSHIFT",  "GLITCHING",   "FUZZING",
    "DUMPING",     "BYPASS",      "SIDECHANNEL", "EXPLOIT",
};

#define WORD_COUNT (sizeof(words) / sizeof(words[0]))
#define MAX_WRONG 7

// ---------------------------------------------------------------------------
// ASCII art hangman stages (0-7 wrong guesses)
// ---------------------------------------------------------------------------
static const char* const hangman_art[8][7] = {
    // 0 wrong
    {"        ",
     "        ",
     "        ",
     "        ",
     "        ",
     "        ",
     "========"},
    // 1
    {"   |    ",
     "   |    ",
     "   |    ",
     "   |    ",
     "   |    ",
     "   |    ",
     "========"},
    // 2
    {"   +--+ ",
     "   |    ",
     "   |    ",
     "   |    ",
     "   |    ",
     "   |    ",
     "========"},
    // 3
    {"   +--+ ",
     "   |  O ",
     "   |    ",
     "   |    ",
     "   |    ",
     "   |    ",
     "========"},
    // 4
    {"   +--+ ",
     "   |  O ",
     "   |  | ",
     "   |    ",
     "   |    ",
     "   |    ",
     "========"},
    // 5
    {"   +--+ ",
     "   |  O ",
     "   | /| ",
     "   |    ",
     "   |    ",
     "   |    ",
     "========"},
    // 6
    {"   +--+ ",
     "   |  O ",
     "   | /|\\",
     "   |    ",
     "   |    ",
     "   |    ",
     "========"},
    // 7 (dead)
    {"   +--+ ",
     "   |  O ",
     "   | /|\\",
     "   | / \\",
     "   |    ",
     "   |    ",
     "========"},
};

// ---------------------------------------------------------------------------
// Main handler
// ---------------------------------------------------------------------------
void hangman_handler(struct command_result* res) {
    if (bp_cmd_help_check(&hangman_def, res->help_flag)) return;

    int term_rows = system_config.terminal_ansi_rows;
    int term_cols = system_config.terminal_ansi_columns;

    game_screen_enter(0);

    game_rng_seed();

    bool quit = false;

    while (!quit) {
        // Pick a random word
        const char* word = words[game_rng_next() % WORD_COUNT];
        int word_len = strlen(word);
        bool guessed[26] = {false};
        int wrong = 0;
        bool solved = false;

        printf("\x1b[2J");
        while (!solved && wrong < MAX_WRONG && !quit) {
            tx_fifo_wait_drain();

            int row = 3;
            int col = (term_cols > 50) ? (term_cols - 40) / 2 : 2;

            // Title
            ui_term_cursor_position(row, col);
            printf("\x1b[1mHangman\x1b[0m  (%d of %d wrong)", wrong, MAX_WRONG);
            row += 2;

            // Draw hangman
            for (int i = 0; i < 7; i++) {
                ui_term_cursor_position(row + i, col);
                printf("%s", hangman_art[wrong][i]);
            }
            row += 8;

            // Draw word with blanks
            ui_term_cursor_position(row, col);
            printf("  ");
            solved = true;
            for (int i = 0; i < word_len; i++) {
                char ch = word[i];
                if (guessed[ch - 'A']) {
                    if (system_config.terminal_ansi_color)
                        printf("\x1b[1;32m%c\x1b[0m ", ch);
                    else
                        printf("%c ", ch);
                } else {
                    printf("_ ");
                    solved = false;
                }
            }
            row += 2;

            // Show guessed letters
            ui_term_cursor_position(row, col);
            printf("Tried: ");
            for (int i = 0; i < 26; i++) {
                if (guessed[i]) {
                    // Check if it's in the word
                    bool in_word = false;
                    for (int j = 0; j < word_len; j++) {
                        if (word[j] == 'A' + i) { in_word = true; break; }
                    }
                    if (in_word) {
                        if (system_config.terminal_ansi_color)
                            printf("\x1b[32m%c\x1b[0m", 'A' + i);
                        else
                            printf("%c", 'A' + i);
                    } else {
                        if (system_config.terminal_ansi_color)
                            printf("\x1b[31m%c\x1b[0m", 'A' + i);
                        else
                            printf("%c", 'a' + i);
                    }
                } else {
                    printf("\xc2\xb7"); // middle dot ·
                }
            }
            row += 2;

            ui_term_cursor_position(row, col);
            printf("Guess a letter (q=quit): ");
            printf("%s", ui_term_cursor_show());

            if (solved) break;

            // Wait for a letter
            bool got = false;
            while (!got && !quit) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; break; }
                    // Convert to uppercase
                    if (c >= 'a' && c <= 'z') c -= 32;
                    if (c >= 'A' && c <= 'Z') {
                        int idx = c - 'A';
                        if (!guessed[idx]) {
                            guessed[idx] = true;
                            // Check if in word
                            bool hit = false;
                            for (int j = 0; j < word_len; j++) {
                                if (word[j] == c) { hit = true; break; }
                            }
                            if (!hit) wrong++;
                            got = true;
                        }
                    }
                }
                busy_wait_ms(10);
            }
            printf("%s", ui_term_cursor_hide());
        }

        if (!quit) {
            tx_fifo_wait_drain();
            // Show result
            int row = term_rows / 2;
            int col = (term_cols > 40) ? (term_cols - 30) / 2 : 2;
            if (solved) {
                ui_term_cursor_position(row, col);
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;32mYou got it! The word was: %s\x1b[0m", word);
                else
                    printf("You got it! The word was: %s", word);
            } else {
                // Show the full hangman and reveal word
                printf("\x1b[2J");
                int r2 = 3;
                int c2 = (term_cols > 50) ? (term_cols - 40) / 2 : 2;
                ui_term_cursor_position(r2, c2);
                printf("\x1b[1mHangman\x1b[0m");
                r2 += 2;
                for (int i = 0; i < 7; i++) {
                    ui_term_cursor_position(r2 + i, c2);
                    printf("%s", hangman_art[MAX_WRONG][i]);
                }
                ui_term_cursor_position(r2 + 8, c2);
                if (system_config.terminal_ansi_color)
                    printf("\x1b[1;31mDead! The word was: %s\x1b[0m", word);
                else
                    printf("Dead! The word was: %s", word);
            }

            // Wait for r or q
            int msg_row = term_rows / 2 + 2;
            if (!solved) msg_row = 14;
            int msg_col = (term_cols > 40) ? (term_cols - 20) / 2 : 2;
            ui_term_cursor_position(msg_row, msg_col);
            printf("r=play again  q=quit");

            bool decided = false;
            while (!decided) {
                char c;
                if (rx_fifo_try_get(&c)) {
                    if (c == 'q' || c == 'Q') { quit = true; decided = true; }
                    if (c == 'r' || c == 'R') { decided = true; }
                }
                busy_wait_ms(10);
            }
        }
    }

    game_screen_exit();
}
