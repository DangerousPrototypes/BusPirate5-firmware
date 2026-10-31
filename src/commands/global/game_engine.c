// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file game_engine.c
 * @brief Shared game engine utilities for Bus Pirate terminal games.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "game_engine.h"

// ---------------------------------------------------------------------------
// PRNG  (xorshift32)
// ---------------------------------------------------------------------------
static uint32_t rng_state;

void game_rng_seed(void) {
    rng_state = (uint32_t)time_us_32();
    if (!rng_state) rng_state = 0xDEADBEEF;
}

uint32_t game_rng_next(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

// ---------------------------------------------------------------------------
// Screen lifecycle
// ---------------------------------------------------------------------------

void game_screen_enter(int scroll_bottom) {
    toolbar_draw_prepare();
    printf("\x1b[?1049h");
    printf("\x1b[2J\x1b[H");
    printf("%s", ui_term_cursor_hide());
    if (scroll_bottom > 0) {
        printf("\x1b[1;%dr", scroll_bottom);
    }
    char drain;
    while (rx_fifo_try_get(&drain)) {}
}

void game_screen_exit(void) {
    char drain;
    while (rx_fifo_try_get(&drain)) {}
    printf("\x1b[r");
    printf("%s", ui_term_cursor_show());
    printf("\x1b[?1049l");
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();
}

void game_set_scroll_region(int bottom) {
    printf("\x1b[1;%dr", bottom);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool game_poll_input(game_input_t *out, uint8_t flags) {
    char c;
    if (!rx_fifo_try_get(&c)) {
        out->type = GAME_INPUT_NONE;
        out->ch = 0;
        return false;
    }
    out->ch = c;

    // ESC sequence → arrow keys
    if (c == 0x1b) {
        char s1, s2;
        busy_wait_ms(2);
        if (rx_fifo_try_get(&s1) && s1 == '[') {
            busy_wait_ms(2);
            if (rx_fifo_try_get(&s2)) {
                switch (s2) {
                    case 'A': out->type = GAME_INPUT_UP; return true;
                    case 'B': out->type = GAME_INPUT_DOWN; return true;
                    case 'C': out->type = GAME_INPUT_RIGHT; return true;
                    case 'D': out->type = GAME_INPUT_LEFT; return true;
                }
            }
        }
        out->type = GAME_INPUT_CHAR;
        return true;
    }

    // WASD → directional (only when flag is set)
    if (flags & GAME_INPUT_WASD) {
        switch (c) {
            case 'w': case 'W': out->type = GAME_INPUT_UP; return true;
            case 'a': case 'A': out->type = GAME_INPUT_LEFT; return true;
            case 's': case 'S': out->type = GAME_INPUT_DOWN; return true;
            case 'd': case 'D': out->type = GAME_INPUT_RIGHT; return true;
        }
    }

    // Quit
    if (c == 'q' || c == 'Q') { out->type = GAME_INPUT_QUIT; return true; }

    // Action (space / enter)
    if (c == ' ' || c == '\r' || c == '\n') { out->type = GAME_INPUT_ACTION; return true; }

    // Everything else
    out->type = GAME_INPUT_CHAR;
    return true;
}

void game_input_drain(void) {
    char c;
    while (rx_fifo_try_get(&c)) {}
}

// ---------------------------------------------------------------------------
// Frame timing
// ---------------------------------------------------------------------------

void game_tick_wait(uint32_t tick_start_us, int tick_ms) {
    uint32_t elapsed_ms = (time_us_32() - tick_start_us) / 1000;
    if ((int)elapsed_ms < tick_ms) {
        busy_wait_ms(tick_ms - (int)elapsed_ms);
    }
}
