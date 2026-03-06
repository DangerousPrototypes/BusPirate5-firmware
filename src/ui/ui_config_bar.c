/**
 * @file ui_config_bar.c
 * @brief Data-driven config bar — rendering and key dispatch.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "ui/ui_config_bar.h"
#include "ui/ui_popup.h"
#include "ui/ui_file_picker.h"
#include "lib/vt100_keys/vt100_keys.h"

/* ── Helpers ────────────────────────────────────────────────────────── */

static void bar_write(ui_config_bar_t *bar, const char *s) {
    bar->write_out(1, s, (int)strlen(s));
}

static void bar_write_buf(ui_config_bar_t *bar, const char *buf, int len) {
    bar->write_out(1, buf, len);
}

/** Is field at index i visible? */
static bool field_visible(ui_config_bar_t *bar, uint8_t i) {
    if (i >= bar->field_count) return false;
    const ui_field_def_t *f = &bar->fields[i];
    if (f->visible && !f->visible(bar->ctx)) return false;
    return true;
}

/** Find the next visible field wrapping around. Returns current if none found. */
static uint8_t find_next_visible(ui_config_bar_t *bar, uint8_t current, int dir) {
    for (uint8_t i = 0; i < bar->field_count; i++) {
        current = (uint8_t)((current + dir + bar->field_count) % bar->field_count);
        if (field_visible(bar, current)) return current;
    }
    return bar->focused; /* no visible field — stay put */
}

/** Get the rendered content width (inside brackets) for a field. */
static int field_content_cols(const ui_field_def_t *f) {
    switch (f->type) {
    case UI_FIELD_SPINNER:
        return f->width > 2 ? f->width : 6;
    case UI_FIELD_CHECKBOX:
        return 1 + (int)strlen(f->checkbox.text ? f->checkbox.text : "");
    case UI_FIELD_NUMBER:
        return f->width ? f->width : 5;
    case UI_FIELD_FILE:
        return f->width ? f->width : 12;
    case UI_FIELD_BUTTON:
        return (int)strlen(f->button.text ? f->button.text : "Go");
    default:
        return f->width ? f->width : 8;
    }
}

/* ── Init ───────────────────────────────────────────────────────────── */

void ui_config_bar_init(ui_config_bar_t *bar,
                        const ui_field_def_t *fields,
                        uint8_t count,
                        void *ctx,
                        uint8_t row,
                        uint8_t cols,
                        uint8_t rows) {
    bar->fields      = fields;
    bar->field_count = count;
    bar->focused     = 0;
    bar->active      = true;
    bar->ctx         = ctx;
    bar->bar_row     = row;
    bar->cols        = cols;
    bar->rows        = rows;
    bar->write_out   = NULL;
    bar->read_key    = NULL;
    bar->repaint     = NULL;

    /* Find first visible field */
    if (count > 0 && !field_visible(bar, 0)) {
        bar->focused = find_next_visible(bar, 0, +1);
    }
}

/* ── Rendering ──────────────────────────────────────────────────────── */

/* Blue toolbar background — matches vt100_menu bar */
#define BAR_BG "\x1b[0;37;44m"

/**
 * Draw a single field as a bracketed widget on the blue toolbar.
 *
 * Focused fields use cyan background (matching active menu tab).
 * Ready buttons use green. Unfocused fields are white-on-blue.
 */
static void draw_field(ui_config_bar_t *bar, const ui_field_def_t *f, bool focused) {
    char buf[40];
    int n;

    /* Only show focus highlight when bar is active */
    bool highlight = focused && bar->active;

    /* Bracket styles — close returns to blue row background */
    const char *open_style;

    switch (f->type) {
    case UI_FIELD_BUTTON: {
        bool ready = !f->button.ready || f->button.ready(bar->ctx);
        if (highlight) {
            open_style = ready ? "\x1b[1;37;42m[" : "\x1b[1;37;46m[";
        } else {
            open_style = ready ? "\x1b[1;32;44m[" : "\x1b[2;37;44m[";
        }
        break;
    }
    default:
        open_style = highlight ? "\x1b[1;37;46m[" : BAR_BG "[";
        break;
    }

    bar_write(bar, " ");
    bar_write(bar, open_style);

    /* Render field content */
    switch (f->type) {
    case UI_FIELD_SPINNER: {
        int idx = f->get_int ? f->get_int(bar->ctx) : -1;
        bool has_val = (idx >= 0 && idx < f->spinner.count && f->spinner.options);
        const char *val = has_val ? f->spinner.options[idx]
                                  : (f->label ? f->label : "------");
        if (!has_val && !highlight) bar_write(bar, "\x1b[2m"); /* dim placeholder */
        n = snprintf(buf, sizeof(buf), "%-*s v", f->width > 2 ? f->width - 2 : 4, val);
        bar_write_buf(bar, buf, n);
        if (!has_val && !highlight) bar_write(bar, BAR_BG);      /* restore */
        break;
    }
    case UI_FIELD_CHECKBOX: {
        int val = f->get_int ? f->get_int(bar->ctx) : 0;
        n = snprintf(buf, sizeof(buf), "%c%s",
                     val ? 'x' : ' ',
                     f->checkbox.text ? f->checkbox.text : "");
        bar_write_buf(bar, buf, n);
        break;
    }
    case UI_FIELD_NUMBER: {
        char tmp[20];
        int val = f->get_int ? f->get_int(bar->ctx) : 0;
        const char *fmt = f->number.fmt ? f->number.fmt : "%d";
        int tlen = snprintf(tmp, sizeof(tmp), fmt, (unsigned)val);
        int w = f->width ? f->width : tlen;
        n = snprintf(buf, sizeof(buf), "%-*s", w, tmp);
        bar_write_buf(bar, buf, n);
        break;
    }
    case UI_FIELD_FILE: {
        const char *val = f->get_str ? f->get_str(bar->ctx) : NULL;
        bool has_val = (val && val[0] != '\0');
        if (!has_val) val = f->label ? f->label : "............";
        if (!has_val && !highlight) bar_write(bar, "\x1b[2m");
        n = snprintf(buf, sizeof(buf), "%-*s", f->width ? f->width : 12, val);
        bar_write_buf(bar, buf, n);
        if (!has_val && !highlight) bar_write(bar, BAR_BG);
        break;
    }
    case UI_FIELD_BUTTON: {
        const char *text = f->button.text ? f->button.text : "Go";
        bar_write(bar, text);
        break;
    }
    }

    bar_write(bar, "]" BAR_BG);
}

void ui_config_bar_draw(ui_config_bar_t *bar) {
    char buf[40];
    int n;

    /* Ensure focused field is visible */
    if (!field_visible(bar, bar->focused)) {
        bar->focused = find_next_visible(bar, bar->focused, +1);
    }

    /* ── Field row (bar_row): blue toolbar ── */
    n = snprintf(buf, sizeof(buf), "\x1b[%d;1H" BAR_BG, bar->bar_row);
    bar_write_buf(bar, buf, n);
    for (uint8_t i = 0; i < bar->field_count; i++) {
        if (!field_visible(bar, i)) continue;
        draw_field(bar, &bar->fields[i], i == bar->focused);
    }

    /* Fill rest of row with blue, then reset */
    bar_write(bar, "\x1b[K\x1b[0m");
}

/* ── Key handling ───────────────────────────────────────────────────── */

/** Handle value change (Up/Down) for the focused field. */
static void field_value_change(ui_config_bar_t *bar, const ui_field_def_t *f, int dir) {
    switch (f->type) {
    case UI_FIELD_SPINNER: {
        int idx = f->get_int ? f->get_int(bar->ctx) : -1;
        int count = f->spinner.count;
        if (count == 0) break;
        if (dir > 0) {
            idx = (idx >= count - 1) ? (f->spinner.wrap ? 0 : count - 1) : idx + 1;
        } else {
            idx = (idx <= 0) ? (f->spinner.wrap ? count - 1 : 0) : idx - 1;
        }
        if (f->set_int) f->set_int(bar->ctx, idx);
        break;
    }
    case UI_FIELD_CHECKBOX: {
        int val = f->get_int ? f->get_int(bar->ctx) : 0;
        if (f->set_int) f->set_int(bar->ctx, !val);
        break;
    }
    case UI_FIELD_NUMBER: {
        int val = f->get_int ? f->get_int(bar->ctx) : 0;
        if (dir > 0) {
            if ((uint32_t)val < f->number.max) val++;
        } else {
            if ((uint32_t)val > f->number.min) val--;
            /* Wrap around */
            else val = (int)f->number.max;
        }
        if ((uint32_t)val > f->number.max) val = (int)f->number.min;
        if (f->set_int) f->set_int(bar->ctx, val);
        break;
    }
    default:
        break;
    }
}

/** Handle Enter/activate for the focused field. */
static void field_activate(ui_config_bar_t *bar, const ui_field_def_t *f) {
    switch (f->type) {
    case UI_FIELD_SPINNER:
        /* Enter on spinner → advance to next field */
        ui_config_bar_focus_next(bar);
        break;

    case UI_FIELD_CHECKBOX:
        /* Toggle */
        field_value_change(bar, f, +1);
        break;

    case UI_FIELD_NUMBER: {
        /* Open number popup */
        ui_popup_io_t pio = {
            .write_out = bar->write_out,
            .cols = bar->cols,
            .rows = bar->rows,
        };
        uint32_t val = (uint32_t)(f->get_int ? f->get_int(bar->ctx) : 0);
        uint32_t result;
        if (ui_popup_number(&pio,
                            f->number.popup_title ? f->number.popup_title : "Enter value",
                            val, f->number.min, f->number.max, &result)) {
            if (f->set_int) f->set_int(bar->ctx, (int)result);
            if (f->on_change) f->on_change(bar->ctx);
        }
        if (bar->repaint) bar->repaint();
        break;
    }

    case UI_FIELD_FILE: {
        /* Open file picker */
        if (bar->read_key) {
            ui_file_picker_io_t fpio = {
                .read_key  = bar->read_key,
                .write_out = bar->write_out,
                .repaint   = bar->repaint,
                .cols      = bar->cols,
                .rows      = bar->rows,
            };
            char file_buf[13] = {0};
            if (ui_file_pick(f->file.ext_filter, file_buf, sizeof(file_buf), &fpio)) {
                if (f->set_str) f->set_str(bar->ctx, file_buf);
                if (f->on_change) f->on_change(bar->ctx);
            }
            if (bar->repaint) bar->repaint();
        }
        break;
    }

    case UI_FIELD_BUTTON:
        if (f->on_activate) f->on_activate(bar->ctx);
        break;
    }
}

bool ui_config_bar_handle_key(ui_config_bar_t *bar, int key) {
    /* Tab → focus next */
    if (key == VT100_KEY_TAB) {
        ui_config_bar_focus_next(bar);
        return true;
    }

    /* Right → focus next */
    if (key == VT100_KEY_RIGHT) {
        ui_config_bar_focus_next(bar);
        return true;
    }

    /* Left → focus prev */
    if (key == VT100_KEY_LEFT) {
        ui_config_bar_focus_prev(bar);
        return true;
    }

    /* Up → value change -1 */
    if (key == VT100_KEY_UP) {
        if (field_visible(bar, bar->focused)) {
            field_value_change(bar, &bar->fields[bar->focused], -1);
        }
        return true;
    }

    /* Down → value change +1 */
    if (key == VT100_KEY_DOWN) {
        if (field_visible(bar, bar->focused)) {
            field_value_change(bar, &bar->fields[bar->focused], +1);
        }
        return true;
    }

    /* Enter → activate */
    if (key == VT100_KEY_ENTER) {
        if (field_visible(bar, bar->focused)) {
            field_activate(bar, &bar->fields[bar->focused]);
        }
        return true;
    }

    /* Space → toggle checkboxes */
    if (key == ' ') {
        if (field_visible(bar, bar->focused) &&
            bar->fields[bar->focused].type == UI_FIELD_CHECKBOX) {
            field_value_change(bar, &bar->fields[bar->focused], +1);
            return true;
        }
        return false; /* not consumed if not a checkbox */
    }

    return false; /* key not consumed */
}

/* ── Focus navigation ───────────────────────────────────────────────── */

void ui_config_bar_focus_next(ui_config_bar_t *bar) {
    bar->focused = find_next_visible(bar, bar->focused, +1);
}

void ui_config_bar_focus_prev(ui_config_bar_t *bar) {
    bar->focused = find_next_visible(bar, bar->focused, -1);
}

uint8_t ui_config_bar_focused(const ui_config_bar_t *bar) {
    return bar->focused;
}
