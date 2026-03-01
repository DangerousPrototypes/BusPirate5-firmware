/*
 * arduboy_term.c — Terminal display/input backend for Arduboy emulator
 *
 * Renders 128×64 1-bit framebuffer using Unicode half-block characters
 * in a VT100/ANSI terminal. Uses 24-bit color (truecolor) escape codes
 * via the Bus Pirate's existing ui_term helpers.
 *
 * Pixel layout:
 *   Each character cell represents 2 vertical pixels. The upper pixel
 *   uses the foreground color and the lower pixel uses the background
 *   color, combined via the '▀' (U+2580) upper-half-block character.
 *
 *   4 possible states per cell:
 *     Both ON  → fg=ON,  bg=ON  → full block '█' (or ▀ with both colors)
 *     Top ON   → fg=ON,  bg=OFF → '▀'
 *     Bot ON   → fg=OFF, bg=ON  → '▄' (lower half block, U+2584)
 *     Both OFF → fg=OFF, bg=OFF → ' '
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include "arduboy_term.h"
#include "pirate.h"
#include "system_config.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_term.h"
#include "lib/vt100_keys/vt100_keys.h"
#include <stdio.h>
#include <string.h>

/* ── Predefined palettes ─────────────────────────────────────── */

const arduboy_palette_t PALETTE_GREEN = { 0, 255, 65,    0, 20, 5 };
const arduboy_palette_t PALETTE_WHITE = { 255, 255, 255,  0, 0, 0 };
const arduboy_palette_t PALETTE_AMBER = { 255, 176, 0,    30, 15, 0 };
const arduboy_palette_t PALETTE_BLUE  = { 100, 180, 255,  0, 10, 30 };

/* ── UTF-8 block characters ──────────────────────────────────── */

/* ▀ U+2580 — upper half block (3 bytes in UTF-8) */
static const char UPPER_HALF[] = "\xe2\x96\x80";
/* ▄ U+2584 — lower half block */
static const char LOWER_HALF[] = "\xe2\x96\x84";
/* █ U+2588 — full block */
static const char FULL_BLOCK[] = "\xe2\x96\x88";

/* ── VT100 key input state (module-level) ────────────────────── */

static vt100_key_state_t key_state;
static bool key_state_initialized = false;

static int key_read_blocking(char* c) {
    rx_fifo_get_blocking(c);
    return 1;
}

static int key_read_try(char* c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}

/* ── Framebuffer pixel access ────────────────────────────────── */

/*
 * Arduboy framebuffer is organized as 8 "pages" of 128 columns.
 * Each byte is a vertical stripe of 8 pixels, LSB at top.
 *
 * To get pixel (x, y):
 *   page = y / 8
 *   bit  = y % 8
 *   byte = fb[page * 128 + x]
 *   pixel = (byte >> bit) & 1
 */
static inline bool fb_pixel(const uint8_t* fb, uint8_t x, uint8_t y) {
    if (x >= ARDUBOY_WIDTH || y >= ARDUBOY_HEIGHT) return false;
    return (fb[(y / 8) * 128 + x] >> (y & 7)) & 1;
}

/* ── Rendering ────────────────────────────────────────────────── */

void arduboy_term_init(arduboy_term_t* term, const arduboy_palette_t* palette,
                       const char* game_name) {
    memset(term, 0, sizeof(*term));
    term->palette = palette ? *palette : PALETTE_GREEN;
    term->game_name = game_name ? game_name : "Arduboy";
    term->first_frame = true;

    /* Initialize key input */
    if (!key_state_initialized) {
        vt100_key_init(&key_state, key_read_blocking, key_read_try);
        key_state_initialized = true;
    }

    /* Calculate centering offsets */
    uint16_t term_cols = system_config.terminal_ansi_columns;
    uint16_t term_rows = system_config.terminal_ansi_rows;
    if (term_cols == 0) term_cols = 80;
    if (term_rows == 0) term_rows = 24;

    term->col_offset = (term_cols > TERM_COLS) ? (term_cols - TERM_COLS) / 2 : 0;
    term->row_offset = (term_rows > TERM_ROWS + 3) ? (term_rows - TERM_ROWS - 3) / 2 : 0;

    /* Enter alternate screen buffer */
    printf("\x1b[?1049h");   /* smcup */
    printf("\x1b[2J");       /* clear screen */
    printf("\x1b[H");        /* cursor home */
    printf("\x1b[?25l");     /* hide cursor */
    printf("\x1b[?7l");      /* disable line wrapping */

    /* Draw border/title */
    uint8_t title_row = term->row_offset;
    ui_term_cursor_position(title_row, term->col_offset);
    printf("\x1b[1m");       /* bold */
    printf("  %s", term->game_name);
    printf("\x1b[0m");       /* reset */

    /* Draw key help below display area */
    uint8_t help_row = term->row_offset + TERM_ROWS + 2;
    ui_term_cursor_position(help_row, term->col_offset);
    printf("\x1b[90m");      /* dim gray */
    printf("  Arrows=DPAD  Z/N=A  X/M=B  Ctrl-Q=Quit");
    printf("\x1b[0m");
}

void arduboy_term_render(arduboy_term_t* term, const uint8_t* fb) {
    const arduboy_palette_t* p = &term->palette;
    uint8_t base_row = term->row_offset + 1; /* +1 for title line */

    /*
     * Key insight: with half-block characters, we only ever need TWO
     * colors — fg=ON_color for lit pixels, bg=OFF_color for dark.
     *   ▀ (U+2580) = top=fg, bottom=bg  → top ON, bottom OFF
     *   ▄ (U+2584) = top=bg, bottom=fg  → top OFF, bottom ON
     *   █ (U+2588) = all fg             → both ON
     *   ' '        = all bg             → both OFF
     *
     * Set colors ONCE and just output characters — reduces output
     * from ~180KB/frame to ~13KB/frame.
     */

    /* Determine which SSD1306 pages changed */
    bool page_dirty[8];
    for (uint8_t pg = 0; pg < 8; pg++) {
        page_dirty[pg] = term->first_frame ||
            memcmp(&fb[pg * 128], &term->prev_fb[pg * 128], 128) != 0;
    }

    /* Set fg=ON color, bg=OFF color — no per-cell changes needed */
    printf("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm",
           p->fg_r, p->fg_g, p->fg_b,
           p->bg_r, p->bg_g, p->bg_b);

    for (uint8_t row = 0; row < TERM_ROWS; row++) {
        /* Each terminal row = 2 pixel rows, both in same SSD1306 page */
        if (!page_dirty[row >> 2]) continue;

        ui_term_cursor_position(base_row + row, term->col_offset + 1);

        uint8_t y_top = row * 2;
        uint8_t y_bot = y_top + 1;

        for (uint8_t col = 0; col < TERM_COLS; col++) {
            bool top = fb_pixel(fb, col, y_top);
            bool bot = fb_pixel(fb, col, y_bot);

            if (top && bot)       printf("%s", FULL_BLOCK);
            else if (top && !bot) printf("%s", UPPER_HALF);
            else if (!top && bot) printf("%s", LOWER_HALF);
            else                  printf(" ");
        }
    }

    /* Reset colors */
    printf("\x1b[0m");

    /* Save current frame for next dirty comparison */
    memcpy(term->prev_fb, fb, ARDUBOY_FB_SIZE);
    term->first_frame = false;

    /* FPS tracking */
    term->fps_counter++;
}

void arduboy_term_status(arduboy_term_t* term, uint32_t millis) {
    /* Update FPS display every second */
    if (millis - term->last_fps_time_ms >= 1000) {
        term->displayed_fps = term->fps_counter;
        term->fps_counter = 0;
        term->last_fps_time_ms = millis;

        uint8_t status_row = term->row_offset + TERM_ROWS + 1;
        ui_term_cursor_position(status_row, term->col_offset);
        printf("\x1b[90m  %lu FPS | %lu ms\x1b[K\x1b[0m",
               (unsigned long)term->displayed_fps,
               (unsigned long)millis);
    }
}

void arduboy_term_cleanup(arduboy_term_t* term) {
    (void)term;
    printf("\x1b[0m");       /* reset colors */
    printf("\x1b[?7h");      /* re-enable line wrapping */
    printf("\x1b[?25h");     /* show cursor */
    printf("\x1b[?1049l");   /* rmcup — leave alternate screen */
}

/* ── Input mapping ────────────────────────────────────────────── */

uint8_t arduboy_term_map_key(int key) {
    switch (key) {
    case VT100_KEY_UP:      return ARDUBOY_BTN_UP;
    case VT100_KEY_DOWN:    return ARDUBOY_BTN_DOWN;
    case VT100_KEY_LEFT:    return ARDUBOY_BTN_LEFT;
    case VT100_KEY_RIGHT:   return ARDUBOY_BTN_RIGHT;
    case 'z': case 'Z':
    case 'n': case 'N':     return ARDUBOY_BTN_A;
    case 'x': case 'X':
    case 'm': case 'M':     return ARDUBOY_BTN_B;
    case VT100_KEY_CTRL_Q:  return 0xFF; /* Exit signal (Ctrl-Q only) */
    default:                return 0;
    }
}

/*
 * Non-blocking input reader for vt100_key_read's blocking callback.
 * Returns the first available byte, or blocks only briefly.
 */
static char nb_first_byte;
static bool nb_have_byte;

static int key_nb_blocking(char* c) {
    /* First call: return the pre-read byte */
    if (nb_have_byte) {
        *c = nb_first_byte;
        nb_have_byte = false;
        return 1;
    }
    /* Shouldn't get here, but read one more byte */
    return rx_fifo_try_get(c) ? 1 : 0;
}

uint8_t arduboy_term_poll_buttons(void) {
    uint8_t buttons = 0;
    char c;

    /*
     * Read available bytes. We peek one byte from the FIFO, set it up
     * as the "first byte" for vt100_key_read's blocking callback, then
     * let vt100_key_read handle escape sequence parsing via try_read
     * for continuation bytes.
     */
    while (rx_fifo_try_get(&c)) {
        nb_first_byte = c;
        nb_have_byte = true;

        /* Temporarily swap in our non-blocking reader */
        vt100_read_blocking_fn saved_blocking = key_state.read_blocking;
        key_state.read_blocking = key_nb_blocking;

        int key = vt100_key_read(&key_state);

        key_state.read_blocking = saved_blocking;

        uint8_t btn = arduboy_term_map_key(key);
        if (btn == 0xFF) return 0xFF; /* Exit */
        buttons |= btn;
    }

    return buttons;
}

/* ── Sticky button hold system ───────────────────────────────── */

/* Button bit → hold timer index mapping */
static const uint8_t btn_bits[BTN_COUNT] = {
    ARDUBOY_BTN_UP, ARDUBOY_BTN_DOWN, ARDUBOY_BTN_LEFT,
    ARDUBOY_BTN_RIGHT, ARDUBOY_BTN_A, ARDUBOY_BTN_B
};

void arduboy_term_update_buttons(arduboy_term_t* term) {
    /* Read all available keypresses */
    uint8_t pressed = arduboy_term_poll_buttons();

    if (pressed == 0xFF) {
        term->exit_requested = true;
        return;
    }

    /* For each detected button press, reset its hold timer */
    for (int i = 0; i < BTN_COUNT; i++) {
        if (pressed & btn_bits[i]) {
            term->btn_hold[i] = BTN_HOLD_FRAMES;
        }
    }
}

uint8_t arduboy_term_get_buttons(arduboy_term_t* term) {
    if (term->exit_requested) return 0xFF;

    /* Build bitmask from all buttons whose hold timer is still active */
    uint8_t buttons = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        if (term->btn_hold[i] > 0) {
            buttons |= btn_bits[i];
        }
    }
    return buttons;
}

void arduboy_term_frame_tick(arduboy_term_t* term) {
    /* Decrement all hold timers by one emulated frame */
    for (int i = 0; i < BTN_COUNT; i++) {
        if (term->btn_hold[i] > 0) {
            term->btn_hold[i]--;
        }
    }
}
