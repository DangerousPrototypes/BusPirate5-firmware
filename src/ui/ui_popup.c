/**
 * @file ui_popup.c
 * @brief Reusable VT100 popup dialogs — confirm, text input, number input.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "usb_rx.h"
#include "ui/ui_popup.h"

/* ── Colour palettes ────────────────────────────────────────────────── */

typedef struct {
    const char *border; /* bold border SGR */
    const char *body;   /* body/text SGR */
    const char *input;  /* input field SGR */
} popup_palette_t;

static const popup_palette_t palettes[] = {
    [UI_POPUP_INFO]   = { "\x1b[1;37;44m", "\x1b[0;37;44m", "\x1b[0;1;37;40m" },
    [UI_POPUP_WARN]   = { "\x1b[1;37;43m", "\x1b[0;30;43m", "\x1b[0;1;37;40m" },
    [UI_POPUP_DANGER] = { "\x1b[1;37;41m", "\x1b[0;37;41m", "\x1b[0;1;37;40m" },
};

/* ── Internal helpers ───────────────────────────────────────────────── */

static void popup_write(const ui_popup_io_t *io, const char *s) {
    io->write_out(1, s, (int)strlen(s));
}

static void popup_write_buf(const ui_popup_io_t *io, const char *buf, int len) {
    io->write_out(1, buf, len);
}

/** Draw a horizontal border row: +---...---+ */
static void popup_hline(const ui_popup_io_t *io, const popup_palette_t *pal,
                         int row, int col, int width) {
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    popup_write_buf(io, buf, n);
    popup_write(io, pal->border);
    popup_write(io, "+");
    for (int i = 0; i < width - 2; i++) popup_write(io, "-");
    popup_write(io, "+");
}

/** Draw a body-coloured text row with border chars on the edges. */
static void popup_text_row(const ui_popup_io_t *io, const popup_palette_t *pal,
                            int row, int col, int width, const char *text) {
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    popup_write_buf(io, buf, n);
    popup_write(io, pal->border);
    popup_write(io, "|");
    popup_write(io, pal->body);
    popup_write(io, " ");

    int text_len = text ? (int)strlen(text) : 0;
    int avail = width - 4; /* border + space on each side */
    if (text && text_len > 0) {
        if (text_len > avail) text_len = avail;
        popup_write_buf(io, text, text_len);
    }
    for (int i = text_len; i < avail; i++) popup_write(io, " ");

    popup_write(io, " ");
    popup_write(io, pal->border);
    popup_write(io, "|");
}

/** Compute centered popup geometry. */
static void popup_geometry(const ui_popup_io_t *io, int popup_w, int popup_h,
                            int *out_left, int *out_top) {
    int left = ((int)io->cols - popup_w) / 2 + 1;
    int top = ((int)io->rows - popup_h) / 2;
    if (left < 1) left = 1;
    if (top < 1) top = 1;
    *out_left = left;
    *out_top = top;
}

/* ── Confirm popup ──────────────────────────────────────────────────── */

bool ui_popup_confirm(const ui_popup_io_t *io,
                      const char *title,
                      const char *message,
                      ui_popup_style_t style) {
    if (style > UI_POPUP_DANGER) style = UI_POPUP_INFO;
    const popup_palette_t *pal = &palettes[style];

    /* Calculate popup width */
    int title_len = title ? (int)strlen(title) : 0;
    int msg_len = message ? (int)strlen(message) : 0;
    int prompt_len = 17; /* "Continue? (y/n) " */
    int content_max = title_len;
    if (msg_len > content_max) content_max = msg_len;
    if (prompt_len > content_max) content_max = prompt_len;
    int popup_w = content_max + 6; /* +4 padding, +2 borders */
    if (popup_w < 30) popup_w = 30;
    if (popup_w > io->cols - 2) popup_w = io->cols - 2;

    /* Height: border + optional title + message + prompt + border */
    int popup_h = 4; /* top border + message + prompt + bottom border */
    if (title) popup_h++;

    int left, top;
    popup_geometry(io, popup_w, popup_h, &left, &top);

    popup_write(io, "\x1b[?25l"); /* hide cursor */

    int r = top;
    popup_hline(io, pal, r++, left, popup_w);
    if (title) {
        popup_text_row(io, pal, r++, left, popup_w, title);
    }
    popup_text_row(io, pal, r++, left, popup_w, message);
    popup_text_row(io, pal, r++, left, popup_w, "Continue? (y/n)");
    popup_hline(io, pal, r, left, popup_w);

    popup_write(io, "\x1b[0m\x1b[?25h"); /* reset + show cursor */

    char c;
    rx_fifo_get_blocking(&c);
    return (c == 'y' || c == 'Y');
}

/* ── Text input popup ───────────────────────────────────────────────── */

/** Check if a character is acceptable given the filter flags. */
static bool char_accepted(char c, uint8_t flags) {
    if ((flags & UI_INPUT_PRINT) && c >= 0x20 && c <= 0x7e) return true;
    if (c >= '0' && c <= '9') return (flags & (UI_INPUT_HEX | UI_INPUT_DEC)) != 0;
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        return (flags & UI_INPUT_HEX) != 0;
    if (c == 'x' || c == 'X') return (flags & UI_INPUT_HEX) != 0;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return (flags & UI_INPUT_ALPHA) != 0;
    if (c == '.') return (flags & UI_INPUT_DOT) != 0;
    return false;
}

bool ui_popup_text_input(const ui_popup_io_t *io,
                         const char *title,
                         const char *prompt,
                         char *buf,
                         uint8_t buf_size,
                         uint8_t flags) {
    const popup_palette_t *pal = &palettes[UI_POPUP_INFO];

    /* Calculate popup width from content */
    int title_len = title ? (int)strlen(title) : 0;
    int prompt_len = prompt ? (int)strlen(prompt) : 0;
    int field_width = buf_size; /* input field visible width */
    if (field_width < 6) field_width = 6;
    int input_row_width = prompt_len + 2 + field_width; /* "prompt: [field]" */
    int content_max = title_len;
    if (input_row_width > content_max) content_max = input_row_width;
    int hint_len = 22; /* "Enter=OK  Esc=Cancel" */
    if (hint_len > content_max) content_max = hint_len;
    int popup_w = content_max + 6;
    if (popup_w < 30) popup_w = 30;
    if (popup_w > io->cols - 2) popup_w = io->cols - 2;

    /* popup_h: top border, title (opt), blank, input row, hint, bottom border */
    int popup_h = 5; /* border + blank + input + hint + border */
    if (title) popup_h++;

    int left, top;
    popup_geometry(io, popup_w, popup_h, &left, &top);

    /* Draw popup chrome */
    popup_write(io, "\x1b[?25l"); /* hide cursor */

    int r = top;
    popup_hline(io, pal, r++, left, popup_w);
    if (title) {
        popup_text_row(io, pal, r++, left, popup_w, title);
    }
    popup_text_row(io, pal, r++, left, popup_w, NULL); /* blank row */

    /* Input field row — drawn manually for the embedded input field */
    int input_row = r++;
    {
        char pos_buf[16];
        int n = snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", input_row, left);
        popup_write_buf(io, pos_buf, n);
        popup_write(io, pal->border);
        popup_write(io, "|");
        popup_write(io, pal->body);
        popup_write(io, " ");
        if (prompt) popup_write(io, prompt);
        popup_write(io, " ");
        popup_write(io, pal->input);
        for (int i = 0; i < field_width; i++) popup_write(io, " ");
        popup_write(io, pal->body);
        /* pad to fill row */
        int used = 1 + prompt_len + 1 + field_width;
        for (int i = used; i < popup_w - 2; i++) popup_write(io, " ");
        popup_write(io, pal->border);
        popup_write(io, "|");
    }

    /* Hint row */
    popup_text_row(io, pal, r++, left, popup_w, "Enter=OK  Esc=Cancel");
    popup_hline(io, pal, r, left, popup_w);

    popup_write(io, "\x1b[0m");

    /* Position cursor in input field */
    int field_col = left + 2 + prompt_len + 1;
    {
        char pos_buf[16];
        int n = snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", input_row, field_col);
        popup_write_buf(io, pos_buf, n);
    }
    popup_write(io, pal->input);
    popup_write(io, "\x1b[?25h"); /* show cursor */

    /* Input loop */
    uint8_t pos = 0;
    memset(buf, 0, buf_size);

    for (;;) {
        char c;
        rx_fifo_get_blocking(&c);

        /* Enter → submit */
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            popup_write(io, "\x1b[0m");
            return (pos > 0);
        }

        /* Escape → cancel */
        if (c == 0x1b) {
            buf[0] = '\0';
            popup_write(io, "\x1b[0m");
            return false;
        }

        /* Backspace */
        if (c == 0x7f || c == '\b') {
            if (pos > 0) {
                pos--;
                char redraw[16];
                int n = snprintf(redraw, sizeof(redraw), "\x1b[%d;%dH", input_row, field_col);
                popup_write_buf(io, redraw, n);
                popup_write(io, pal->input);
                popup_write_buf(io, buf, pos);
                popup_write(io, " ");
                for (int i = pos + 1; i < field_width; i++) popup_write(io, " ");
                n = snprintf(redraw, sizeof(redraw), "\x1b[%d;%dH", input_row, field_col + pos);
                popup_write_buf(io, redraw, n);
            }
            continue;
        }

        /* Character input */
        if (pos < buf_size - 1 && char_accepted(c, flags)) {
            buf[pos++] = c;
            char echo[2] = { c, 0 };
            popup_write(io, echo);
        }
    }
}

/* ── Number input popup ─────────────────────────────────────────────── */

bool ui_popup_number(const ui_popup_io_t *io,
                     const char *title,
                     uint32_t default_val,
                     uint32_t min_val,
                     uint32_t max_val,
                     uint32_t *result) {
    char prompt_buf[32];
    snprintf(prompt_buf, sizeof(prompt_buf), "[0x%lX]", (unsigned long)default_val);

    char input_buf[12] = {0};
    bool ok = ui_popup_text_input(io, title, prompt_buf, input_buf, sizeof(input_buf),
                                  UI_INPUT_HEX);
    if (!ok) {
        /* Empty submit → use default */
        if (input_buf[0] == '\0') {
            *result = default_val;
            return true;
        }
        return false;
    }

    /* Parse hex or decimal */
    uint32_t val;
    if (input_buf[0] == '0' && (input_buf[1] == 'x' || input_buf[1] == 'X')) {
        val = (uint32_t)strtoul(&input_buf[2], NULL, 16);
    } else {
        val = (uint32_t)strtoul(input_buf, NULL, 0);
    }

    if (val < min_val || val > max_val) {
        return false;
    }

    *result = val;
    return true;
}

/* ── Progress / operation popup ─────────────────────────────────────── */

/**
 * Redraw a single text row inside the progress popup.
 * Clears to the popup width (no trailing garbage).
 */
static void pp_draw_row(const ui_popup_progress_t *pp,
                        int row, const char *sgr, const char *text) {
    const ui_popup_io_t *io = pp->io;
    const popup_palette_t *pal = &palettes[UI_POPUP_INFO];
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, pp->left);
    popup_write_buf(io, buf, n);
    popup_write(io, pal->border);
    popup_write(io, "|");
    popup_write(io, sgr ? sgr : pal->body);
    popup_write(io, " ");

    int avail = pp->width - 4;
    int len = text ? (int)strlen(text) : 0;
    if (len > avail) len = avail;
    if (text && len > 0) popup_write_buf(io, text, len);
    for (int i = len; i < avail; i++) popup_write(io, " ");

    popup_write(io, " ");
    popup_write(io, pal->border);
    popup_write(io, "|");
    popup_write(io, "\x1b[0m");
}

/**
 * Redraw the progress bar row.
 */
static void pp_draw_progress(const ui_popup_progress_t *pp) {
    const ui_popup_io_t *io = pp->io;
    const popup_palette_t *pal = &palettes[UI_POPUP_INFO];
    int avail = pp->width - 4;
    char line[120];

    float pct = (pp->prog_total > 0)
              ? (float)pp->prog_cur / (float)pp->prog_total
              : 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    int bar_w = avail - 12; /* " [####...] NNN% " */
    if (bar_w < 10) bar_w = 10;
    if (bar_w > avail - 8) bar_w = avail - 8;
    int filled = (int)(pct * bar_w);

    /* Build the bar string: " [####    ] NNN%" */
    char *p = line;
    *p++ = '[';
    for (int i = 0; i < bar_w; i++) *p++ = (i < filled) ? '#' : ' ';
    p += snprintf(p, sizeof(line) - (size_t)(p - line),
                  "] %3d%%", (int)(pct * 100));
    *p = '\0';

    /* Position and draw */
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", pp->row_progress, pp->left);
    popup_write_buf(io, buf, n);
    popup_write(io, pal->border);
    popup_write(io, "|");
    popup_write(io, "\x1b[0;1;37;44m"); /* bold white on blue */
    popup_write(io, " ");

    int len = (int)strlen(line);
    if (len > avail) len = avail;
    popup_write_buf(io, line, len);
    for (int i = len; i < avail; i++) popup_write(io, " ");

    popup_write(io, " ");
    popup_write(io, pal->border);
    popup_write(io, "|");
    popup_write(io, "\x1b[0m");
}

void ui_popup_progress_open(ui_popup_progress_t *pp,
                            const ui_popup_io_t *io,
                            const char *title) {
    memset(pp, 0, sizeof(*pp));
    pp->io = io;

    /* Sizing: wide enough for a useful progress bar */
    int content_w = (int)io->cols - 8;
    if (content_w < 40) content_w = 40;
    if (content_w > 72) content_w = 72;
    pp->width = content_w + 4; /* +4 for borders and padding */

    /* Height: border + title + progress + message + warning + error +
     *         result + hint + border = 9 rows */
    pp->height = 9;

    popup_geometry(io, pp->width, pp->height, &pp->left, &pp->top);

    /* Assign row numbers */
    int r = pp->top;
    /* r+0 = top border */
    pp->row_title    = r + 1;
    pp->row_progress = r + 2;
    pp->row_message  = r + 3;
    pp->row_warning  = r + 4;
    pp->row_error    = r + 5;
    pp->row_result   = r + 6;
    pp->row_hint     = r + 7;
    /* r+8 = bottom border */

    /* Draw the full popup frame */
    const popup_palette_t *pal = &palettes[UI_POPUP_INFO];

    popup_write(io, "\x1b[?25l"); /* hide cursor */
    popup_hline(io, pal, pp->top, pp->left, pp->width);
    pp_draw_row(pp, pp->row_title, "\x1b[0;1;37;44m", title);
    pp_draw_progress(pp);
    pp_draw_row(pp, pp->row_message, NULL, NULL);
    pp_draw_row(pp, pp->row_warning, NULL, NULL);
    pp_draw_row(pp, pp->row_error, NULL, NULL);
    pp_draw_row(pp, pp->row_result, NULL, NULL);
    pp_draw_row(pp, pp->row_hint, "\x1b[0;2;37;44m", "Operation in progress...");
    popup_hline(io, pal, pp->top + pp->height - 1, pp->left, pp->width);
    popup_write(io, "\x1b[0m");
}

void ui_popup_progress_set_progress(ui_popup_progress_t *pp,
                                    uint32_t current, uint32_t total) {
    pp->prog_cur = current;
    pp->prog_total = total;
    pp_draw_progress(pp);
}

void ui_popup_progress_set_message(ui_popup_progress_t *pp, const char *msg) {
    /* Only draw the first message — it has the useful detail.
     * Later calls (e.g. "Write complete") are redundant with the
     * result row shown by _finish(). */
    if (pp->msg_set) return;
    char buf[80] = {0};
    if (msg) {
        while (*msg == '\r' || *msg == '\n') msg++;
        strncpy(buf, msg, sizeof(buf) - 1);
        char *end = buf + strlen(buf);
        while (end > buf && (end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
    }
    if (buf[0]) pp->msg_set = true;
    pp_draw_row(pp, pp->row_message, "\x1b[0;37;44m", buf);
}

void ui_popup_progress_set_warning(ui_popup_progress_t *pp, const char *msg) {
    char buf[80] = {0};
    if (msg) {
        while (*msg == '\r' || *msg == '\n') msg++;
        strncpy(buf, msg, sizeof(buf) - 1);
        char *end = buf + strlen(buf);
        while (end > buf && (end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
    }
    pp_draw_row(pp, pp->row_warning, "\x1b[0;1;33;44m", buf);
}

void ui_popup_progress_set_error(ui_popup_progress_t *pp, const char *msg) {
    char buf[80] = {0};
    if (msg) {
        while (*msg == '\r' || *msg == '\n') msg++;
        strncpy(buf, msg, sizeof(buf) - 1);
        char *end = buf + strlen(buf);
        while (end > buf && (end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
    }
    pp_draw_row(pp, pp->row_error, "\x1b[0;1;31;44m", buf);
}

void ui_popup_progress_finish(ui_popup_progress_t *pp,
                              bool success, const char *result_msg) {
    pp->finished = true;
    pp->success = success;

    /* Show result row in green or red */
    const char *sgr = success ? "\x1b[0;1;32;44m" : "\x1b[0;1;31;44m";
    pp_draw_row(pp, pp->row_result, sgr, result_msg ? result_msg : "Done");

    /* Update hint row */
    pp_draw_row(pp, pp->row_hint, "\x1b[0;2;37;44m", "Press any key...");

    /* Redraw border in green/red to signal done */
    const popup_palette_t done_pal = success
        ? (popup_palette_t){ "\x1b[1;32;44m", "\x1b[0;37;44m", "\x1b[0;1;37;40m" }
        : (popup_palette_t){ "\x1b[1;31;44m", "\x1b[0;37;44m", "\x1b[0;1;37;40m" };
    popup_hline(pp->io, &done_pal, pp->top, pp->left, pp->width);
    popup_hline(pp->io, &done_pal, pp->top + pp->height - 1, pp->left, pp->width);
    popup_write(pp->io, "\x1b[0m");
}

bool ui_popup_progress_wait(ui_popup_progress_t *pp) {
    (void)pp;
    char c;
    rx_fifo_get_blocking(&c);
    return (c == 0x1b);
}
