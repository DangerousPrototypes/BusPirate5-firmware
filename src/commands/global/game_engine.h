// Copyright (c) 2025 Bus Pirate (http://buspirate.com)
// SPDX-License-Identifier: MIT

/**
 * @file game_engine.h
 * @brief Shared game engine utilities for Bus Pirate terminal games.
 *
 * Centralises the common boilerplate every game needs: PRNG,
 * fullscreen lifecycle (alt-screen, toolbar, cursor, scroll region),
 * input polling with unified arrow/WASD mapping, and frame timing.
 *
 * ## Anti-flicker rendering rule
 *
 * NEVER erase then redraw — overwrite in place instead.
 *
 * - **Turn-based games**: clear screen ONCE before the game loop.
 *   Inside the loop, use cursor positioning to overwrite content.
 *   Append \x1b[K after variable-length text lines to trim leftovers.
 *
 * - **Real-time games**: build a line buffer (char[] + color[]) per row,
 *   stamp all objects into it, then emit the whole row in one pass.
 *   End each row with \x1b[0m\x1b[K. Never clear rows before drawing.
 *   See crossflash.c emit_row() for the reference pattern.
 *
 * - One-shot clears for level transitions or game-over screens are OK.
 */

#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// PRNG  (xorshift32 — replaces 14 per-game copies)
// ---------------------------------------------------------------------------
void game_rng_seed(void);
uint32_t game_rng_next(void);

// ---------------------------------------------------------------------------
// Screen lifecycle
// ---------------------------------------------------------------------------

/**
 * Enter fullscreen game mode.
 *
 * Saves toolbar state, enters alt screen, clears screen, hides cursor,
 * optionally sets a scroll region, and drains pending input.
 *
 * @param scroll_bottom  Last scrollable row (1-based).
 *                       Pass 0 to skip scroll-region setup.
 */
void game_screen_enter(int scroll_bottom);

/**
 * Exit fullscreen game mode.
 *
 * Resets scroll region, shows cursor, exits alt screen, restores
 * toolbar, and repositions the prompt cursor.
 */
void game_screen_exit(void);

/**
 * Update the scroll region mid-game (e.g. after a level resize).
 *
 * @param bottom  New last scrollable row (1-based).
 */
void game_set_scroll_region(int bottom);

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

typedef enum {
    GAME_INPUT_NONE = 0,
    GAME_INPUT_UP,
    GAME_INPUT_DOWN,
    GAME_INPUT_LEFT,
    GAME_INPUT_RIGHT,
    GAME_INPUT_QUIT,     // q / Q
    GAME_INPUT_ACTION,   // SPACE, ENTER
    GAME_INPUT_CHAR,     // everything else — raw char in .ch
} game_input_type_t;

typedef struct {
    game_input_type_t type;
    char ch;             // raw character (always populated)
} game_input_t;

/** If set, w/a/s/d are mapped to UP/LEFT/DOWN/RIGHT. */
#define GAME_INPUT_WASD  (1 << 0)

/**
 * Poll one input event from the USB FIFO.
 *
 * Arrow-key ESC sequences are always decoded.  WASD mapping is
 * controlled by \p flags.  q/Q always maps to GAME_INPUT_QUIT;
 * SPACE/ENTER to GAME_INPUT_ACTION.  Anything else is
 * GAME_INPUT_CHAR with the raw byte in \c out->ch.
 *
 * @param out    Receives the decoded event.
 * @param flags  Bitmask of GAME_INPUT_* flags.
 * @return true if an event was available, false otherwise.
 */
bool game_poll_input(game_input_t *out, uint8_t flags);

/** Drain all pending bytes from the input FIFO. */
void game_input_drain(void);

// ---------------------------------------------------------------------------
// Frame timing
// ---------------------------------------------------------------------------

/**
 * Sleep for the remainder of the current frame.
 *
 * Call at the end of the game loop:
 * @code
 *     uint32_t t0 = time_us_32();
 *     // ... input, update, draw ...
 *     game_tick_wait(t0, TICK_MS);
 * @endcode
 *
 * @param tick_start_us  Value from time_us_32() at frame start.
 * @param tick_ms        Desired frame period in milliseconds.
 */
void game_tick_wait(uint32_t tick_start_us, int tick_ms);

#endif // GAME_ENGINE_H
