/*
 * arduboy_term.h — Terminal display/input backend for Arduboy emulator
 *
 * Renders the 128×64 1-bit Arduboy framebuffer as ANSI art in the
 * Bus Pirate USB serial terminal, and maps keyboard input to Arduboy
 * buttons via the vt100_keys decoder.
 *
 * Uses Unicode half-block character (▀, U+2580) to pack 2 vertical
 * pixels per character cell, resulting in a 128×32 character grid.
 * With dirty-cell tracking, only changed cells are redrawn.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef ARDUBOY_TERM_H
#define ARDUBOY_TERM_H

#include <stdint.h>
#include <stdbool.h>
#include "arduboy_periph.h"

/* ── Terminal display dimensions ──────────────────────────────── */
#define TERM_COLS       128     /* Characters wide (1 pixel = 1 char column) */
#define TERM_ROWS       32      /* Character rows (2 pixels per row via ▀) */

/* ── Color palette ───────────────────────────────────────────── */
typedef struct {
    uint8_t fg_r, fg_g, fg_b;  /* "On" pixel color (foreground) */
    uint8_t bg_r, bg_g, bg_b;  /* "Off" pixel color (background) */
} arduboy_palette_t;

/* Predefined palettes */
extern const arduboy_palette_t PALETTE_GREEN;   /* Classic green phosphor */
extern const arduboy_palette_t PALETTE_WHITE;   /* White on black */
extern const arduboy_palette_t PALETTE_AMBER;   /* Amber monochrome */
extern const arduboy_palette_t PALETTE_BLUE;    /* Blue on dark */

/* ── Button hold configuration ───────────────────────────────── */
/* Number of emulated frames a button stays "held" after a keypress.
 * Arduboy runs at 60fps internally, so 8 frames ≈ 133ms hold time.
 * This bridges the gap between terminal discrete key events and the
 * game's expectation of reading held button state via pollButtons(). */
#define BTN_HOLD_FRAMES  8
#define BTN_COUNT        6

/* ── Terminal renderer state ─────────────────────────────────── */

typedef struct {
    /* Previous frame for dirty-cell detection */
    uint8_t prev_fb[ARDUBOY_FB_SIZE];
    bool first_frame;

    /* Current palette */
    arduboy_palette_t palette;

    /* Status bar */
    const char* game_name;
    uint32_t fps_counter;
    uint32_t last_fps_time_ms;
    uint32_t displayed_fps;

    /* Rendering offset (center in terminal) */
    uint8_t row_offset;
    uint8_t col_offset;

    /* Sticky button hold timers (frames remaining) */
    uint8_t btn_hold[BTN_COUNT];   /* UP, DOWN, LEFT, RIGHT, A, B */
    bool exit_requested;

} arduboy_term_t;

/* ── Public API ───────────────────────────────────────────────── */

/**
 * Initialize terminal renderer. Sets up alternate screen, hides cursor.
 */
void arduboy_term_init(arduboy_term_t* term, const arduboy_palette_t* palette,
                       const char* game_name);

/**
 * Render a framebuffer to the terminal. Only redraws changed cells.
 * @param term  Terminal state
 * @param fb    1024-byte Arduboy framebuffer (column-major pages)
 */
void arduboy_term_render(arduboy_term_t* term, const uint8_t* fb);

/**
 * Draw/update the status bar below the game display.
 */
void arduboy_term_status(arduboy_term_t* term, uint32_t millis);

/**
 * Restore terminal state (show cursor, leave alternate screen).
 */
void arduboy_term_cleanup(arduboy_term_t* term);

/**
 * Map a vt100_key_read() keycode to an Arduboy button bitmask.
 * Returns 0 if the key doesn't map to a button.
 * Special return: 0xFF means "exit requested" (ESC or Ctrl-Q).
 */
uint8_t arduboy_term_map_key(int key);

/**
 * Poll for input (non-blocking). Returns combined button bitmask.
 * Reads all available keys from the input FIFO.
 */
uint8_t arduboy_term_poll_buttons(void);

/**
 * Drain the input FIFO and update sticky hold timers.
 * Call this frequently (e.g. mid-frame) to catch keypresses promptly.
 * @param term  Terminal state with hold timers
 */
void arduboy_term_update_buttons(arduboy_term_t* term);

/**
 * Get the current effective button state (with sticky holds).
 * Returns combined bitmask. 0xFF if exit was requested.
 */
uint8_t arduboy_term_get_buttons(arduboy_term_t* term);

/**
 * Tick hold timers down by one frame. Call once per emulated frame.
 */
void arduboy_term_frame_tick(arduboy_term_t* term);

#endif /* ARDUBOY_TERM_H */
