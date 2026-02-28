/*
 * vt100_keys.c — Unified VT100 terminal key decoder
 *
 * Single CSI escape-sequence parser shared by all Bus Pirate fullscreen apps.
 * Replaces 6 independent copy-pasted parsers (linenoise, kilo, hx, menu_demo,
 * logic_bar, scope) with one well-tested implementation.
 *
 * Handles:
 *   ESC [ A/B/C/D/H/F         — arrows, Home, End
 *   ESC [ 1~..8~              — Home, Insert, Delete, End, PgUp, PgDn
 *   ESC [ 11~..24~            — F1–F12 (two-digit codes)
 *   ESC O H/F/P/Q/R/S         — Home, End, F1–F4 (SS3 sequences)
 *   Bare ESC                  — detected via non-blocking read timeout
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include "vt100_keys.h"
#include <string.h>

void vt100_key_init(vt100_key_state_t* s, vt100_read_blocking_fn read_blocking, vt100_read_try_fn read_try) {
    memset(s, 0, sizeof(*s));
    s->read_blocking = read_blocking;
    s->read_try = read_try;
    s->pushback = -1;
}

void vt100_key_unget(vt100_key_state_t* s, int key) {
    s->pushback = key;
}

/**
 * Map a single-digit CSI numeric code (ESC [ N ~) to a virtual key.
 */
static int decode_csi_tilde_1(int n) {
    switch (n) {
        case 1: return VT100_KEY_HOME;
        case 2: return VT100_KEY_INSERT;
        case 3: return VT100_KEY_DELETE;
        case 4: return VT100_KEY_END;
        case 5: return VT100_KEY_PAGEUP;
        case 6: return VT100_KEY_PAGEDOWN;
        case 7: return VT100_KEY_HOME; /* rxvt */
        case 8: return VT100_KEY_END;  /* rxvt */
        default: return VT100_KEY_ESC;
    }
}

/**
 * Map a two-digit CSI numeric code (ESC [ NN ~) to a virtual key.
 * Covers F1–F12 across various terminal emulators.
 */
static int decode_csi_tilde_2(int code) {
    switch (code) {
        case 11: return VT100_KEY_F1;
        case 12: return VT100_KEY_F2;
        case 13: return VT100_KEY_F3;
        case 14: return VT100_KEY_F4;
        case 15: return VT100_KEY_F5;
        case 17: return VT100_KEY_F6;  /* note: 16 is skipped */
        case 18: return VT100_KEY_F7;
        case 19: return VT100_KEY_F8;
        case 20: return VT100_KEY_F9;
        case 21: return VT100_KEY_F10;
        case 23: return VT100_KEY_F11; /* note: 22 is skipped */
        case 24: return VT100_KEY_F12;
        default: return VT100_KEY_ESC;
    }
}

/**
 * Try to read one byte.  Uses non-blocking read_try if available,
 * otherwise falls back to blocking read.
 *
 * Returns 1 if a byte was read, 0 if nothing available (non-blocking only).
 */
static int try_read(vt100_key_state_t* s, char* c) {
    if (s->read_try) {
        return s->read_try(c);
    }
    /* No non-blocking reader — fall back to blocking */
    return (s->read_blocking(c) == 1) ? 1 : 0;
}

/* ── Stateless sequence decoder (shared by vt100_key_read and linenoise) ── */

int vt100_key_decode_csi(const char* seq, int len) {
    if (len < 2) {
        return VT100_KEY_ESC;
    }

    /* ESC [ — CSI sequences */
    if (seq[0] == '[') {
        /* ESC [ letter — simple cursor keys */
        if (seq[1] >= 'A' && seq[1] <= 'Z') {
            switch (seq[1]) {
                case 'A': return VT100_KEY_UP;
                case 'B': return VT100_KEY_DOWN;
                case 'C': return VT100_KEY_RIGHT;
                case 'D': return VT100_KEY_LEFT;
                case 'H': return VT100_KEY_HOME;
                case 'F': return VT100_KEY_END;
                default: return VT100_KEY_ESC;
            }
        }

        /* ESC [ digit ... — numeric CSI */
        if (seq[1] >= '0' && seq[1] <= '9') {
            if (len >= 3 && seq[2] == '~') {
                /* Single-digit: ESC [ N ~ */
                return decode_csi_tilde_1(seq[1] - '0');
            }
            if (len >= 4 && seq[2] >= '0' && seq[2] <= '9' && seq[3] == '~') {
                /* Two-digit: ESC [ NN ~ */
                int code = (seq[1] - '0') * 10 + (seq[2] - '0');
                return decode_csi_tilde_2(code);
            }
        }
    }

    /* ESC O — SS3 sequences (some terminals use these for Home/End/F1-F4) */
    if (seq[0] == 'O') {
        switch (seq[1]) {
            case 'H': return VT100_KEY_HOME;
            case 'F': return VT100_KEY_END;
            case 'P': return VT100_KEY_F1;
            case 'Q': return VT100_KEY_F2;
            case 'R': return VT100_KEY_F3;
            case 'S': return VT100_KEY_F4;
            default: return VT100_KEY_ESC;
        }
    }

    /* Unrecognised sequence */
    return VT100_KEY_ESC;
}

/* ── Full I/O-integrated key reader ──────────────────────────────── */

int vt100_key_read(vt100_key_state_t* s) {
    /* Return pushed-back key first */
    if (s->pushback >= 0) {
        int k = s->pushback;
        s->pushback = -1;
        return k;
    }

    char c;
    if (s->read_blocking(&c) != 1) {
        return -1; /* read error */
    }

    /* Non-escape characters pass through directly */
    if ((unsigned char)c != VT100_KEY_ESC) {
        return (unsigned char)c;
    }

    /* ESC was read — try to get the next byte.
     * If nothing follows, it's a bare ESC keypress. */
    char seq[4];
    int seqlen = 0;
    if (!try_read(s, &seq[0])) {
        return VT100_KEY_ESC; /* bare ESC */
    }
    seqlen = 1;
    if (!try_read(s, &seq[1])) {
        return VT100_KEY_ESC; /* incomplete sequence */
    }
    seqlen = 2;

    /* Numeric CSI: may need a 3rd or 4th byte */
    if (seq[0] == '[' && seq[1] >= '0' && seq[1] <= '9') {
        if (try_read(s, &seq[2])) {
            seqlen = 3;
            if (seq[2] >= '0' && seq[2] <= '9') {
                if (try_read(s, &seq[3])) {
                    seqlen = 4;
                }
            }
        }
    }

    return vt100_key_decode_csi(seq, seqlen);
}
