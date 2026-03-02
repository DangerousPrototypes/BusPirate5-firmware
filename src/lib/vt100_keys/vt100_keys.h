/*
 * vt100_keys.h — Unified VT100 terminal key decoder
 *
 * Provides a single shared escape-sequence parser and virtual key-code enum
 * for all Bus Pirate fullscreen apps (edit, hexedit, scope, menu_demo, etc).
 *
 * Usage:
 *   1. Provide blocking and (optionally) non-blocking read callbacks
 *   2. Call vt100_key_init() once
 *   3. Call vt100_key_read() in your input loop — returns raw ASCII bytes
 *      for printable/control keys, or VT100_KEY_* codes for special keys
 *   4. Use vt100_key_unget() for one-key pushback (e.g. menu passthrough)
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef VT100_KEYS_H
#define VT100_KEYS_H

#include <stdint.h>

/* ── Virtual key codes ───────────────────────────────────────────────
 * 0x00–0x7F  : raw ASCII / control characters (pass through unchanged)
 * 0x100+     : virtual keys (escape sequences decoded to these)
 * Using 0x100 base avoids collision with any raw byte.              */
enum vt100_key {
    /* Control characters — keep raw values for direct comparison */
    VT100_KEY_CTRL_A    = 0x01,
    VT100_KEY_CTRL_B    = 0x02,
    VT100_KEY_CTRL_C    = 0x03,
    VT100_KEY_CTRL_D    = 0x04,
    VT100_KEY_CTRL_E    = 0x05,
    VT100_KEY_CTRL_F    = 0x06,
    VT100_KEY_CTRL_H    = 0x08,
    VT100_KEY_TAB       = 0x09,
    VT100_KEY_CTRL_K    = 0x0b,
    VT100_KEY_CTRL_L    = 0x0c,
    VT100_KEY_ENTER     = 0x0d,
    VT100_KEY_CTRL_N    = 0x0e,
    VT100_KEY_CTRL_P    = 0x10,
    VT100_KEY_CTRL_Q    = 0x11,
    VT100_KEY_CTRL_R    = 0x12,
    VT100_KEY_CTRL_S    = 0x13,
    VT100_KEY_CTRL_T    = 0x14,
    VT100_KEY_CTRL_U    = 0x15,
    VT100_KEY_CTRL_W    = 0x17,
    VT100_KEY_ESC       = 0x1b,
    VT100_KEY_BACKSPACE = 0x7f,

    /* Virtual keys (decoded from escape sequences) */
    VT100_KEY_UP        = 0x100,
    VT100_KEY_DOWN      = 0x101,
    VT100_KEY_RIGHT     = 0x102,
    VT100_KEY_LEFT      = 0x103,
    VT100_KEY_HOME      = 0x104,
    VT100_KEY_END       = 0x105,
    VT100_KEY_DELETE    = 0x106,
    VT100_KEY_INSERT    = 0x107,
    VT100_KEY_PAGEUP    = 0x108,
    VT100_KEY_PAGEDOWN  = 0x109,

    /* Function keys */
    VT100_KEY_F1        = 0x110,
    VT100_KEY_F2        = 0x111,
    VT100_KEY_F3        = 0x112,
    VT100_KEY_F4        = 0x113,
    VT100_KEY_F5        = 0x114,
    VT100_KEY_F6        = 0x115,
    VT100_KEY_F7        = 0x116,
    VT100_KEY_F8        = 0x117,
    VT100_KEY_F9        = 0x118,
    VT100_KEY_F10       = 0x119,
    VT100_KEY_F11       = 0x11a,
    VT100_KEY_F12       = 0x11b,
};

/* ── I/O callback signatures ─────────────────────────────────────── */

/**
 * Blocking read: must read exactly 1 byte into *c.
 * Return 1 on success, -1 on error.
 */
typedef int (*vt100_read_blocking_fn)(char* c);

/**
 * Non-blocking read: try to read 1 byte into *c.
 * Return 1 if a byte was read, 0 if nothing available.
 */
typedef int (*vt100_read_try_fn)(char* c);

/* ── Decoder state ───────────────────────────────────────────────── */

typedef struct {
    vt100_read_blocking_fn read_blocking;
    vt100_read_try_fn read_try;
    int pushback; /* -1 = empty */
} vt100_key_state_t;

/* ── Public API ──────────────────────────────────────────────────── */

/**
 * Initialise decoder state.
 *
 * @param s              State struct to initialise.
 * @param read_blocking  Required. Blocks until one byte is available.
 * @param read_try       Optional (may be NULL). Used for timeout-based
 *                       bare-ESC detection.  If NULL, ESC followed by
 *                       an unrecognised byte is returned as VT100_KEY_ESC
 *                       using the blocking read.
 */
void vt100_key_init(vt100_key_state_t* s, vt100_read_blocking_fn read_blocking, vt100_read_try_fn read_try);

/**
 * Read one key (blocking).  Returns a raw ASCII byte or a VT100_KEY_* code.
 */
int vt100_key_read(vt100_key_state_t* s);

/**
 * Push back one key code so the next vt100_key_read() returns it immediately.
 */
void vt100_key_unget(vt100_key_state_t* s, int key);

/**
 * Decode a CSI/SS3 escape sequence from pre-read bytes.
 *
 * @param seq   Bytes after ESC (caller already consumed the ESC).
 *              seq[0] is '[' or 'O', seq[1..] are the payload.
 * @param len   Number of valid bytes in seq (typically 2–4).
 * @return      VT100_KEY_* code, or VT100_KEY_ESC if unrecognised.
 *
 * This is a pure function with no side effects — no I/O, no state.
 * It exists so callers that manage their own byte reads (e.g. linenoise)
 * can share the sequence-to-keycode lookup tables without adopting the
 * full vt100_key_read() I/O model.
 */
int vt100_key_decode_csi(const char* seq, int len);

/**
 * Decode a key from a first byte + rx_fifo.
 *
 * @param first_byte  The byte already consumed from the input FIFO.
 *                    If ESC (0x1b), remaining CSI bytes are read from
 *                    rx_fifo_try_get().
 * @return            VT100_KEY_* code, or raw ASCII byte.
 *
 * This is designed for the toolbar focus state machine in pirate.c where
 * the first byte is already consumed via rx_fifo_try_get() before key
 * decoding begins.  Uses only non-blocking reads so it never stalls.
 */
int vt100_key_read_rx_fifo(char first_byte);

#endif /* VT100_KEYS_H */
