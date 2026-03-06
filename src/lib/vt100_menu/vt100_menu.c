/*
 * vt100_menu.c — VT100 top menu bar with dropdown selection framework
 *
 * Pure VT100 escape-sequence rendering — no platform dependencies beyond
 * the read_key / write_out callbacks supplied at init time.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include "vt100_menu.h"

#include <string.h>
#include <stdio.h>

/* ── Internal helpers ───────────────────────────────────────────────── */

/* Size for temporary escape-sequence scratch buffers (stack-allocated). */
#define OUT_BUF_SIZE 256

/**
 * Write a formatted string to the terminal via the state's write callback.
 */
static void menu_write(const vt100_menu_state_t* s, const char* data, int len) {
    if (s->write_out && len > 0) {
        s->write_out(1, data, len);
    }
}

static void menu_writes(const vt100_menu_state_t* s, const char* str) {
    menu_write(s, str, (int)strlen(str));
}

/**
 * Move cursor to (row, col) — 1-based.
 */
static void menu_goto(const vt100_menu_state_t* s, int row, int col) {
    char out_buf[32];
    int n = snprintf(out_buf, sizeof(out_buf), "\x1b[%d;%dH", row, col);
    menu_write(s, out_buf, n);
}

/**
 * Save cursor position.
 */
static void menu_cursor_save(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[s");
}

/**
 * Restore cursor position.
 */
static void menu_cursor_restore(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[u");
}

/**
 * Hide cursor.
 */
static void menu_cursor_hide(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[?25l");
}

/**
 * Show cursor.
 */
static void menu_cursor_show(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[?25h");
}

/**
 * Set normal (default) text attributes.
 */
static void menu_attr_reset(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0m");
}

/**
 * Menu bar normal style: white text on dark blue background.
 */
static void menu_attr_bar(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0;37;44m");
}

/**
 * Menu bar highlighted tab style: bold white on cyan.
 */
static void menu_attr_bar_selected(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[1;37;46m");
}

/**
 * Dropdown normal item: black text on white/light-grey background.
 */
static void menu_attr_dropdown(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0;30;47m");
}

/**
 * Dropdown highlighted item: white on blue.
 */
static void menu_attr_dropdown_selected(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[1;37;44m");
}

/**
 * Dropdown disabled item: dark grey on white.
 */
static void menu_attr_dropdown_disabled(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0;90;47m");
}

/**
 * Dropdown separator: same as dropdown bg.
 */
static void menu_attr_dropdown_separator(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0;30;47m");
}

/**
 * Right-aligned shortcut hint style: dimmer text.
 */
static void menu_attr_shortcut(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0;90;47m");
}

static void menu_attr_shortcut_selected(const vt100_menu_state_t* s) {
    menu_writes(s, "\x1b[0;36;44m");
}

/* ── Geometry helpers ───────────────────────────────────────────────── */

/**
 * Calculate the column where a top-level menu label starts.
 * Menus are laid out as: " File  Edit  Search  Help "
 * with 2 spaces between each.
 */
static int menu_bar_col(const vt100_menu_state_t* s, int menu_idx) {
    int col = 2;  /* 1-based + 1 leading space */
    for (int i = 0; i < menu_idx && i < s->menu_count; i++) {
        col += (int)strlen(s->menus[i].label) + 2;  /* label + 2 padding */
    }
    return col;
}

/**
 * Calculate the width needed for a dropdown menu.
 * Width = max(label_len + shortcut_len + padding) across all items,
 * minimum 12 characters.
 */
static int dropdown_width(const vt100_menu_def_t* menu) {
    int max_w = 12;
    for (int i = 0; i < menu->count; i++) {
        const vt100_menu_item_t* item = &menu->items[i];
        if (item->flags & MENU_ITEM_SEPARATOR) continue;
        int w = (int)strlen(item->label) + 2;  /* 1 space padding each side */
        if (item->shortcut) {
            w += (int)strlen(item->shortcut) + 2;  /* gap + shortcut */
        }
        if (w > max_w) max_w = w;
    }
    return max_w;
}

/**
 * Find the next selectable item in a direction (1 = down, -1 = up).
 * Skips separators and disabled items.  Wraps around.
 */
static int8_t next_selectable(const vt100_menu_def_t* menu, int8_t from, int dir) {
    if (menu->count == 0) return -1;
    int8_t idx = from;
    for (int attempts = 0; attempts < menu->count; attempts++) {
        idx += dir;
        if (idx < 0) idx = menu->count - 1;
        if (idx >= menu->count) idx = 0;
        const vt100_menu_item_t* item = &menu->items[idx];
        if (!(item->flags & (MENU_ITEM_SEPARATOR | MENU_ITEM_DISABLED))) {
            return idx;
        }
    }
    return -1;  /* nothing selectable */
}

/* ── Rendering ──────────────────────────────────────────────────────── */

/**
 * Draw the menu bar across the full width of the terminal.
 */
static void draw_bar(const vt100_menu_state_t* s) {
    char out_buf[OUT_BUF_SIZE];
    menu_goto(s, s->bar_row, 1);
    menu_attr_bar(s);

    /* Fill entire row with menu bar background */
    memset(out_buf, ' ', s->screen_cols < OUT_BUF_SIZE ? s->screen_cols : OUT_BUF_SIZE - 1);
    menu_write(s, out_buf, s->screen_cols < OUT_BUF_SIZE ? s->screen_cols : OUT_BUF_SIZE - 1);

    /* Draw each menu label */
    int col = 2;
    for (int i = 0; i < s->menu_count; i++) {
        menu_goto(s, s->bar_row, col);
        if (s->active && i == s->selected_menu) {
            menu_attr_bar_selected(s);
        } else {
            menu_attr_bar(s);
        }
        int n = snprintf(out_buf, OUT_BUF_SIZE, " %s ", s->menus[i].label);
        menu_write(s, out_buf, n);
        col += (int)strlen(s->menus[i].label) + 2;
    }

    /* Right-aligned hint when bar is not in interactive mode */
    if (!s->active) {
        const char* hint = "F10=Menu";
        int hint_len = (int)strlen(hint);
        int hint_col = s->screen_cols - hint_len;
        if (hint_col > col) {
            menu_goto(s, s->bar_row, hint_col);
            menu_attr_bar(s);
            menu_writes(s, hint);
        }
    }

    menu_attr_reset(s);
}

/**
 * Draw the dropdown box for the currently selected menu.
 *
 * The dropdown is drawn as an overlay — content beneath it will
 * need to be repainted after the menu closes.
 */
static void draw_dropdown(const vt100_menu_state_t* s) {
    const vt100_menu_def_t* menu = &s->menus[s->selected_menu];
    int left_col = menu_bar_col(s, s->selected_menu) - 1;
    int width = dropdown_width(menu);
    int top_row = s->bar_row + 1;

    /* Clamp to screen right edge */
    if (left_col + width + 2 > s->screen_cols) {
        left_col = s->screen_cols - width - 2;
        if (left_col < 1) left_col = 1;
    }

    /* Top border */
    menu_goto(s, top_row, left_col);
    menu_attr_dropdown(s);
    {
        char out_buf[4];
        int n = snprintf(out_buf, sizeof(out_buf), "+");
        menu_write(s, out_buf, n);
        for (int j = 0; j < width; j++) menu_writes(s, "-");
        menu_writes(s, "+");
    }

    /* Menu items */
    for (int i = 0; i < menu->count; i++) {
        int row = top_row + 1 + i;
        const vt100_menu_item_t* item = &menu->items[i];

        menu_goto(s, row, left_col);

        if (item->flags & MENU_ITEM_SEPARATOR) {
            menu_attr_dropdown_separator(s);
            menu_writes(s, "+");
            for (int j = 0; j < width; j++) menu_writes(s, "-");
            menu_writes(s, "+");
            continue;
        }

        bool is_selected = (i == s->selected_item);
        bool is_disabled = (item->flags & MENU_ITEM_DISABLED) != 0;

        if (is_selected) {
            menu_attr_dropdown_selected(s);
        } else if (is_disabled) {
            menu_attr_dropdown_disabled(s);
        } else {
            menu_attr_dropdown(s);
        }

        menu_writes(s, "|");

        /* Checked mark */
        if (item->flags & MENU_ITEM_CHECKED) {
            menu_writes(s, "*");
        } else {
            menu_writes(s, " ");
        }

        /* Label */
        int label_len = (int)strlen(item->label);
        menu_writes(s, item->label);

        /* Padding between label and shortcut */
        int shortcut_len = item->shortcut ? (int)strlen(item->shortcut) : 0;
        int pad = width - label_len - shortcut_len - 2;  /* -1 check, -1 trailing space */
        if (pad < 0) pad = 0;
        for (int j = 0; j < pad; j++) menu_writes(s, " ");

        /* Shortcut hint */
        if (item->shortcut) {
            if (is_selected) {
                menu_attr_shortcut_selected(s);
            } else {
                menu_attr_shortcut(s);
            }
            menu_writes(s, item->shortcut);
            /* Restore item attribute for trailing border */
            if (is_selected) {
                menu_attr_dropdown_selected(s);
            } else if (is_disabled) {
                menu_attr_dropdown_disabled(s);
            } else {
                menu_attr_dropdown(s);
            }
        }

        menu_writes(s, " |");
    }

    /* Bottom border */
    int bottom_row = top_row + 1 + menu->count;
    menu_goto(s, bottom_row, left_col);
    menu_attr_dropdown(s);
    menu_writes(s, "+");
    for (int j = 0; j < width; j++) menu_writes(s, "-");
    menu_writes(s, "+");

    menu_attr_reset(s);
}

/**
 * Erase the area occupied by the current dropdown.
 * Writes spaces in the default attribute so the caller can repaint.
 */
static void erase_dropdown(const vt100_menu_state_t* s) {
    const vt100_menu_def_t* menu = &s->menus[s->selected_menu];
    int left_col = menu_bar_col(s, s->selected_menu) - 1;
    int width = dropdown_width(menu) + 2;  /* +2 for borders */
    int top_row = s->bar_row + 1;
    int total_rows = menu->count + 2;  /* items + top/bottom borders */

    if (left_col + width > s->screen_cols) {
        left_col = s->screen_cols - width;
        if (left_col < 1) left_col = 1;
    }

    menu_attr_reset(s);
    for (int r = 0; r < total_rows; r++) {
        char out_buf[16];
        menu_goto(s, top_row + r, left_col);
        /* Erase to end of that region with spaces */
        int n = snprintf(out_buf, sizeof(out_buf), "\x1b[%dX", width);
        menu_write(s, out_buf, n);
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

void vt100_menu_init(vt100_menu_state_t* state,
                     const vt100_menu_def_t* menus,
                     uint8_t menu_count,
                     uint8_t bar_row,
                     uint8_t cols,
                     uint8_t rows,
                     int (*read_key_fn)(void),
                     int (*write_fn)(int, const void*, int)) {
    memset(state, 0, sizeof(*state));
    state->menus = menus;
    state->menu_count = menu_count;
    state->bar_row = bar_row;
    state->screen_cols = cols;
    state->screen_rows = rows;
    state->read_key = read_key_fn;
    state->write_out = write_fn;
    state->selected_menu = 0;
    state->selected_item = -1;
    state->active = false;
    state->dropdown_open = false;

    /* Key code defaults — callers can override after init() */
    state->key_up    = VT100_MENU_KEY_UP;
    state->key_down  = VT100_MENU_KEY_DOWN;
    state->key_left  = VT100_MENU_KEY_LEFT;
    state->key_right = VT100_MENU_KEY_RIGHT;
    state->key_enter = VT100_MENU_KEY_ENTER;
    state->key_esc   = VT100_MENU_KEY_ESC;
    state->key_f10   = VT100_MENU_KEY_F10;
}

void vt100_menu_draw_bar(const vt100_menu_state_t* state) {
    menu_cursor_save(state);
    menu_cursor_hide(state);
    draw_bar(state);
    menu_cursor_restore(state);
    /* Do NOT re-show cursor — caller manages visibility */
}

void vt100_menu_erase(const vt100_menu_state_t* state) {
    char out_buf[8];
    menu_goto(state, state->bar_row, 1);
    menu_attr_reset(state);
    int n = snprintf(out_buf, sizeof(out_buf), "\x1b[2K");  /* erase entire line */
    menu_write(state, out_buf, n);
}

uint8_t vt100_menu_reserved_rows(const vt100_menu_state_t* state) {
    /* We always reserve row 1 for the menu bar when the system is in use */
    return (state->menus != NULL) ? 1 : 0;
}

int vt100_menu_run(vt100_menu_state_t* state) {
    if (!state->menus || state->menu_count == 0 || !state->read_key) {
        return MENU_RESULT_CANCEL;
    }

    state->active = true;
    state->dropdown_open = true;
    state->selected_item = -1;
    state->unhandled_key = 0;

    /* Select first valid item in the first menu */
    const vt100_menu_def_t* cur_menu = &state->menus[state->selected_menu];
    state->selected_item = next_selectable(cur_menu, -1, 1);

    menu_cursor_save(state);
    menu_cursor_hide(state);

    /* Initial draw */
    draw_bar(state);
    draw_dropdown(state);

    /* Interactive loop */
    int result = MENU_RESULT_NONE;
    while (result == MENU_RESULT_NONE) {
        int key = state->read_key();
        cur_menu = &state->menus[state->selected_menu];

        if (key == state->key_esc || key == state->key_f10) {
            /* Close menu */
            result = MENU_RESULT_CANCEL;

        } else if (key == state->key_left) {
            /* Move to previous top-level menu */
            if (state->selected_menu == 0) {
                state->selected_menu = state->menu_count - 1;
            } else {
                state->selected_menu--;
            }
            cur_menu = &state->menus[state->selected_menu];
            state->selected_item = next_selectable(cur_menu, -1, 1);
            /* Repaint editor content to restore area under old dropdown */
            if (state->repaint) {
                state->repaint();
                menu_cursor_hide(state);  /* repaint may re-show cursor */
            } else {
                erase_dropdown(state);
            }
            draw_bar(state);
            draw_dropdown(state);

        } else if (key == state->key_right) {
            /* Move to next top-level menu */
            state->selected_menu = (state->selected_menu + 1) % state->menu_count;
            cur_menu = &state->menus[state->selected_menu];
            state->selected_item = next_selectable(cur_menu, -1, 1);
            /* Repaint editor content to restore area under old dropdown */
            if (state->repaint) {
                state->repaint();
                menu_cursor_hide(state);  /* repaint may re-show cursor */
            } else {
                erase_dropdown(state);
            }
            draw_bar(state);
            draw_dropdown(state);

        } else if (key == state->key_down) {
            /* Move down in dropdown */
            int8_t next = next_selectable(cur_menu, state->selected_item, 1);
            if (next >= 0) {
                state->selected_item = next;
                draw_dropdown(state);
            }

        } else if (key == state->key_up) {
            /* Move up in dropdown */
            int8_t next = next_selectable(cur_menu, state->selected_item, -1);
            if (next >= 0) {
                state->selected_item = next;
                draw_dropdown(state);
            }

        } else if (key == state->key_enter) {
            /* Select current item */
            if (state->selected_item >= 0 && state->selected_item < cur_menu->count) {
                const vt100_menu_item_t* item = &cur_menu->items[state->selected_item];
                if (!(item->flags & (MENU_ITEM_SEPARATOR | MENU_ITEM_DISABLED))) {
                    result = item->action_id;
                }
            }

        } else {
            /* Check for shortcut key — single character matching a label's
             * first letter (case-insensitive accelerator).
             * Also check if the key matches any item's shortcut hint. */
            int orig_key = key;
            if (key >= 'a' && key <= 'z') key -= 32;  /* toupper */
            bool matched = false;
            for (int i = 0; i < cur_menu->count; i++) {
                const vt100_menu_item_t* item = &cur_menu->items[i];
                if (item->flags & (MENU_ITEM_SEPARATOR | MENU_ITEM_DISABLED)) continue;
                char first = item->label[0];
                if (first >= 'a' && first <= 'z') first -= 32;
                if (first == key) {
                    result = item->action_id;
                    matched = true;
                    break;
                }
            }
            /* Key not consumed — pass it through to the application */
            if (!matched) {
                state->unhandled_key = orig_key;
                result = MENU_RESULT_PASSTHROUGH;
            }
        }
    }

    /* Clean up: erase dropdown overlay, restore cursor */
    if (state->dropdown_open) {
        erase_dropdown(state);
        state->dropdown_open = false;
    }
    state->active = false;

    /* Erase the bar too — let the caller decide whether to redraw it */
    menu_attr_reset(state);

    menu_cursor_restore(state);
    menu_cursor_show(state);

    if (result == MENU_RESULT_PASSTHROUGH) return MENU_RESULT_PASSTHROUGH;
    return (result > 0) ? result : MENU_RESULT_REDRAW;
}
