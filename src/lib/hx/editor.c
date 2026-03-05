/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */

#include "editor.h"
#include "util.h"
#include "undo.h"

#ifdef BUSPIRATE
#include "lib/vt100_menu/vt100_menu.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef BUSPIRATE
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ── Paged-mode convenience macros ──
 * E_FILE_LEN: total file size (= content_length when not paged)
 * E_PAGE_OFF: file offset of first byte in contents[]
 * These compile down to direct field accesses with no overhead. */
#ifdef BUSPIRATE
#define E_FILE_LEN(e)  ((e)->paged ? (e)->file_size : (e)->content_length)
#define E_PAGE_OFF(e)  ((e)->page_offset)
#else
#define E_FILE_LEN(e)  ((e)->content_length)
#define E_PAGE_OFF(e)  (0u)
#endif

/* Hex column attribute states (state-tracked to minimise VT100 output) */
#define HX_ATTR_NONE   0  /* default / after \x1b[0m */
#define HX_ATTR_PRINT  1  /* \x1b[1;34m  — printable byte (bold blue) */
#define HX_ATTR_CURSOR 2  /* \x1b[7m     — cursor (reverse video) */

/* ASCII column attribute states */
#define HX_ATTR_A_PRINT    1  /* \x1b[33m — yellow (printable) */
#define HX_ATTR_A_NONPRINT 2  /* \x1b[36m — cyan   (non-printable) */
#define HX_ATTR_A_CURSOR   3  /* \x1b[7;36m — reverse + cyan (cursor, non-printable) */
#define HX_ATTR_A_CURSOR_P 4  /* \x1b[7;33m — reverse + yellow (cursor, printable) */

/*
 * This function looks convoluted as hell, but it works...
 */
void editor_move_cursor(struct editor* e, int dir, int amount) {
	switch (dir) {
	case KEY_UP:    e->cursor_y-=amount; break;
	case KEY_DOWN:  e->cursor_y+=amount; break;
	case KEY_LEFT:  e->cursor_x-=amount; break;
	case KEY_RIGHT: e->cursor_x+=amount; break;
	}
	// Did we hit the start of the file? If so, stop moving and place
	// the cursor on the top-left of the hex display.
	if (e->cursor_x <= 1 && e->cursor_y <= 1 && e->line <= 0) {
		e->cursor_x = 1;
		e->cursor_y = 1;
		return;
	}

	// Move the cursor over the x (columns) axis.
	if (e->cursor_x < 1) {
		// Are we trying to move left on the leftmost boundary?
		// Then we go up a row, cursor to the right. Like a text editor.
		if (e->cursor_y >= 1) {
			e->cursor_y--;
			e->cursor_x = e->octets_per_line;
		}
	} else if (e->cursor_x > e->octets_per_line) {
		// Moving to the rightmost boundary?
		// Then move a line down, position the cursor to the beginning of the row.
		// Unless it's the end of file.
		e->cursor_y++;
		e->cursor_x = 1;
	}

	// Did we try to move up when there's nothing?
	// Then stop moving upwards, do not scroll, return.
	if (e->cursor_y <= 1 && e->line <= 0) {
		e->cursor_y = 1;
	}

	// Move the cursor over the y axis
	if (e->cursor_y > e->screen_rows - e->header_rows - 1) {
		e->cursor_y = e->screen_rows - e->header_rows - 1;
		editor_scroll(e, 1);
	} else if (e->cursor_y < 1 && e->line > 0) {
		e->cursor_y = 1;
		editor_scroll(e, -1);
	}

	// Did we hit the end of the file somehow? Set the cursor position
	// to the maximum cursor position possible.
	unsigned int offset = editor_offset_at_cursor(e);
	unsigned int file_len = E_FILE_LEN(e);
	if (file_len > 0 && offset >= file_len - 1) {
		editor_cursor_at_offset(e, offset, &e->cursor_x, &e->cursor_y);
		return;
	}
}

void editor_newfile(struct editor* e, const char* filename) {
	e->filename = malloc(strlen(filename) + 1);
	if (e->filename == NULL) {
		perror("Could not allocate memory for the filename");
		abort();
	}
	strncpy(e->filename, filename, strlen(filename) + 1);
	e->contents = malloc(0);
	e->content_length = 0;
}

void editor_openfile(struct editor* e, const char* filename) {
#ifdef BUSPIRATE
	/* --- Bus Pirate: use FatFS via hx_compat shims --- */
	FSIZE_t fsize = 0;
	int open_result = hx_file_open_read(filename, &fsize);
	if (open_result < 0) {
		if (open_result == -2) {
			/* file does not exist, open as new file */
			editor_newfile(e, filename);
			return;
		}
		editor_statusmessage(e, STATUS_ERROR, "Unable to open '%s'", filename);
		return;
	}

	/* Store the filename regardless of mode */
	e->filename = malloc(strlen(filename) + 1);
	if (e->filename == NULL) {
		perror("Could not allocate memory for the filename");
		hx_file_close_read();
		abort();
	}
	strncpy(e->filename, filename, strlen(filename) + 1);

	/* ── Paged mode for large files ── */
	if ((unsigned int)fsize > HX_PAGED_THRESHOLD) {
		hx_file_close_read();  /* close; page loads use hx_file_read_at */
		e->paged = true;
		e->file_size = (unsigned int)fsize;
		e->page_size = HX_PAGE_SIZE;
		e->page_offset = 0;

		/* Keep a copy of the path for page reloads */
		e->paged_path = malloc(strlen(filename) + 1);
		if (e->paged_path) strncpy(e->paged_path, filename, strlen(filename) + 1);

		/* Allocate fixed page buffer */
		e->contents = malloc(e->page_size);
		if (e->contents == NULL) {
			perror("Could not allocate paged buffer");
			abort();
		}

		/* Load first page */
		int bytes_read = hx_file_read_at(filename, 0, e->contents, e->page_size);
		e->content_length = (bytes_read > 0) ? (unsigned int)bytes_read : 0;

		editor_statusmessage(e, STATUS_WARNING,
			"\"%s\" (%u bytes) [READ-ONLY, PAGE 0]", e->filename, e->file_size);
		return;
	}

	/* ── Normal (full-load) mode ── */
	char* contents = NULL;
	unsigned int content_length = 0;

	if (fsize > 0) {
		contents = malloc((size_t)fsize);
		if (contents == NULL) {
			perror("Could not allocate memory for the file");
			hx_file_close_read();
			abort();
		}
		int bytes_read = hx_file_read(contents, (size_t)fsize);
		if (bytes_read < 0) {
			editor_statusmessage(e, STATUS_ERROR, "Unable to read '%s'", filename);
			free(contents);
			hx_file_close_read();
			return;
		}
		content_length = (unsigned int)bytes_read;
	} else {
		contents = malloc(0);
		content_length = 0;
	}
	hx_file_close_read();

	e->contents = contents;
	e->content_length = content_length;

	editor_statusmessage(e, STATUS_INFO, "\"%s\" (%d bytes)", e->filename, e->content_length);
#else
	/* --- Original POSIX implementation --- */
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL) {
		if (errno == ENOENT) {
			// file does not exist, open it as a new file and return.
			editor_newfile(e, filename);
			return;
		}

		// Other errors (i.e. permission denied). Exit prematurely,
		// no use in continuing.
		perror("Unable to open file");
		exit(1);
	}

	// stat() the file.
	struct stat statbuf;
	if (stat(filename, &statbuf) == -1) {
		perror("Cannot stat file");
		exit(1);
	}
	// S_ISREG is a a POSIX macro to check whether the given st_mode
	// denotes a regular file. See `man 2 stat'.
	if (!S_ISREG(statbuf.st_mode)) {
		fprintf(stderr, "File '%s' is not a regular file\n", filename);
		exit(1);
	}

	// The content buffer.
	char* contents;
	int content_length = 0;

	if (statbuf.st_size <= 0) {
		// The stat() returned a (less than) zero size length. This may be
		// because the user is trying to read a file from /proc/. In that
		// case, read the data per-byte until EOF.
		struct charbuf* buf = charbuf_create();
		int c;
		char tempbuf[1];
		while ((c = fgetc(fp)) != EOF) {
			tempbuf[0] = (char) c;
			charbuf_append(buf, tempbuf, 1);
		}
		// Point contents to the charbuf's contents and set the length accordingly.
		contents = buf->contents;
		content_length = buf->len;
	} else {
		contents = malloc(sizeof(char) * statbuf.st_size);
		if (contents == NULL) {
			perror("Could not allocate memory for the file specified");
			abort();
		}
		content_length = statbuf.st_size;

		// fread() has a massive performance improvement when reading large files.
		if (fread(contents, 1, statbuf.st_size, fp) < (size_t) statbuf.st_size) {
			perror("Unable to read file contents");
			free(contents);
			exit(1);
		}
	}

	// duplicate string without using gnu99 strdup().
	e->filename = malloc(strlen(filename) + 1);
	if (e->filename == NULL) {
		perror("Could not allocate memory for the filename");
		abort();
	}
	strncpy(e->filename, filename, strlen(filename) + 1);
	e->contents = contents;
	e->content_length = content_length;

	// Check if the file is readonly, and warn the user about that.
	if (access(filename, W_OK) == -1) {
		editor_statusmessage(e, STATUS_WARNING, "\"%s\" (%d bytes) [readonly]", e->filename, e->content_length);
	} else {
		editor_statusmessage(e, STATUS_INFO, "\"%s\" (%d bytes)", e->filename, e->content_length);
	}

	if (fclose(fp) != 0) {
		perror("Could not close file properly");
		abort();
	}
#endif /* BUSPIRATE */
}

/* ── Paged-mode page loader ──
 * Re-reads a chunk of the file centred around `target_offset`.
 * Re-uses the existing e->contents buffer (fixed size = e->page_size). */
#ifdef BUSPIRATE
void editor_load_page(struct editor* e, unsigned int target_offset) {
	if (!e->paged || !e->paged_path) return;

	/* Centre the page around the target, clamped to file boundaries */
	unsigned int half = e->page_size / 2;
	unsigned int new_offset = (target_offset > half) ? (target_offset - half) : 0;
	/* Align to octets_per_line so address gutters are tidy */
	if (e->octets_per_line > 0)
		new_offset -= new_offset % (unsigned int)e->octets_per_line;
	if (new_offset + e->page_size > e->file_size && e->file_size > e->page_size)
		new_offset = e->file_size - e->page_size;

	int bytes_read = hx_file_read_at(e->paged_path, new_offset,
					 e->contents, e->page_size);
	if (bytes_read < 0) {
		editor_statusmessage(e, STATUS_ERROR, "Page read error at 0x%x", new_offset);
		return;
	}
	e->page_offset = new_offset;
	e->content_length = (unsigned int)bytes_read;
}

/* Ensure the visible range [file_offset .. file_offset+need) is within the
 * currently loaded page.  If not, reload. */
static void editor_ensure_page(struct editor* e, unsigned int file_offset, unsigned int need) {
	if (!e->paged) return;
	if (file_offset >= e->page_offset &&
	    file_offset + need <= e->page_offset + e->content_length) {
		return;  /* already loaded */
	}
	editor_load_page(e, file_offset);
}
#endif /* BUSPIRATE */

void editor_writefile(struct editor* e) {
	assert(e->filename != NULL);

#ifdef BUSPIRATE
	int result = hx_file_write_all(e->filename, e->contents, e->content_length);
	if (result < 0) {
		editor_statusmessage(e, STATUS_ERROR, "Unable to write '%s'", e->filename);
		return;
	}
	editor_statusmessage(e, STATUS_INFO, "\"%s\", %d bytes written", e->filename, e->content_length);
	e->dirty = false;
#else
	FILE* fp = fopen(e->filename, "wb");
	if (fp == NULL) {
		editor_statusmessage(e, STATUS_ERROR, "Unable to open '%s' for writing: %s", e->filename, strerror(errno));
		return;
	}

	size_t bw = fwrite(e->contents, sizeof(char), e->content_length, fp);
	if (bw <= 0) {
		editor_statusmessage(e, STATUS_ERROR, "Unable write to file: %s", strerror(errno));
		return;
	}

	editor_statusmessage(e, STATUS_INFO, "\"%s\", %d bytes written", e->filename, e->content_length);
	e->dirty = false;

	if (fclose(fp) != 0) {
		perror("Could not close file properly");
		abort();
	}
#endif
}


void editor_cursor_at_offset(struct editor* e, int offset, int* x, int* y) {
	*x = offset % e->octets_per_line + 1;
	*y = offset / e->octets_per_line - e->line + 1;
}


void editor_delete_char_at_cursor(struct editor* e) {
	unsigned int offset = editor_offset_at_cursor(e);
	unsigned int old_length = e->content_length;

	if (e->content_length <= 0) {
		editor_statusmessage(e, STATUS_WARNING, "Nothing to delete");
		return;
	}

	unsigned char charat = e->contents[offset];
	editor_delete_char_at_offset(e, offset);
	e->dirty = true;

	// if the deleted offset was the maximum offset, move the cursor to
	// the left.
	if (offset >= old_length - 1) {
		editor_move_cursor(e, KEY_LEFT, 1);
	}
	if (e->undo_list) action_list_add(e->undo_list, ACTION_DELETE, offset, charat);
}

void editor_delete_char_at_offset(struct editor* e, unsigned int offset) {
	// Remove an element from the contents buffer by moving memory.
	memmove(e->contents + offset, e->contents + offset + 1 , e->content_length - offset - 1);
	e->contents = realloc(e->contents, e->content_length - 1);
	e->content_length--;
}

void editor_increment_byte(struct editor* e, int amount) {
	unsigned int offset = editor_offset_at_cursor(e);
	unsigned char prev = e->contents[offset];
	e->contents[offset] += amount;

	if (e->undo_list) action_list_add(e->undo_list, ACTION_REPLACE, offset, prev);
}


inline int editor_offset_at_cursor(struct editor* e) {
	unsigned int file_len = E_FILE_LEN(e);
	unsigned int offset = (e->cursor_y - 1 + e->line) * e->octets_per_line + (e->cursor_x - 1);
	if (offset <= 0) {
		return 0;
	}
	if (offset >= file_len) {
		return file_len - 1;
	}
	return offset;
}


void editor_scroll(struct editor* e, int units) {
	e->line += units;

	unsigned int file_len = E_FILE_LEN(e);
	int upper_limit = file_len / e->octets_per_line - (e->screen_rows - e->header_rows - 2);
	if (e->line >= upper_limit) {
		e->line = upper_limit;
	}

	if (e->line <= 0) {
		e->line = 0;
	}
}

void editor_scroll_to_offset(struct editor* e, unsigned int offset) {
	unsigned int file_len = E_FILE_LEN(e);
	if (offset > file_len) {
		editor_statusmessage(e, STATUS_ERROR, "Out of range: 0x%09x (%u)", offset, offset);
		return;
	}

	unsigned int offset_min = e->line * e->octets_per_line;
	unsigned int offset_max = offset_min + ((e->screen_rows - e->header_rows) * e->octets_per_line);

	if (offset >= offset_min && offset <= offset_max) {
		editor_cursor_at_offset(e, offset, &(e->cursor_x), &(e->cursor_y));
		return;
	}

	e->line = offset / e->octets_per_line - ((e->screen_rows - e->header_rows) / 2);

	int upper_limit = file_len / e->octets_per_line - (e->screen_rows - e->header_rows - 2);
	if (e->line >= upper_limit) {
		e->line = upper_limit;
	}

	if (e->line <= 0) {
		e->line = 0;
	}

	editor_cursor_at_offset(e, offset, &(e->cursor_x), &(e->cursor_y));
}

void editor_setmode(struct editor* e, enum editor_mode mode) {
	e->mode = mode;
	switch (e->mode) {
	case MODE_NORMAL:        editor_statusmessage(e, STATUS_INFO, ""); break;
	case MODE_APPEND:        editor_statusmessage(e, STATUS_INFO, "-- APPEND -- "); break;
	case MODE_APPEND_ASCII:  editor_statusmessage(e, STATUS_INFO, "-- APPEND ASCII --"); break;
	case MODE_REPLACE_ASCII: editor_statusmessage(e, STATUS_INFO, "-- REPLACE ASCII --"); break;
	case MODE_INSERT:        editor_statusmessage(e, STATUS_INFO, "-- INSERT --"); break;
	case MODE_INSERT_ASCII:  editor_statusmessage(e, STATUS_INFO, "-- INSERT ASCII --"); break;
	case MODE_REPLACE:       editor_statusmessage(e, STATUS_INFO, "-- REPLACE --"); break;
	case MODE_COMMAND: break;
	case MODE_SEARCH:  break;
	}
}


int editor_statusmessage(struct editor* e, enum status_severity sev, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int x = vsnprintf(e->status_message, sizeof(e->status_message), fmt, ap);
	if (x < 0) {
		fprintf(stderr, "Unable to format string");
		abort();
	}
	va_end(ap);

	e->status_severity = sev;

	return x;
}

void editor_render_ascii(struct editor* e, int rownum, unsigned int start_offset, struct charbuf* b) {
	int cc = 0;
	int attr = HX_ATTR_NONE;
	unsigned int file_len = E_FILE_LEN(e);
	unsigned int pg = E_PAGE_OFF(e);

	for (unsigned int offset = start_offset; offset < start_offset + e->octets_per_line; offset++) {
		if (offset >= file_len) {
			break;
		}
		cc++;

		char c = e->contents[offset - pg];
		int desired;
		if (rownum == e->cursor_y && cc == e->cursor_x) {
			desired = isprint(c) ? HX_ATTR_A_CURSOR_P : HX_ATTR_A_CURSOR;
		} else if (isprint(c)) {
			desired = HX_ATTR_A_PRINT;
		} else {
			desired = HX_ATTR_A_NONPRINT;
		}

		/* Emit escape only on attribute transition */
		if (desired != attr) {
			if (attr != HX_ATTR_NONE) charbuf_append(b, "\x1b[0m", 4);
			switch (desired) {
			case HX_ATTR_A_PRINT:    charbuf_append(b, "\x1b[33m", 5); break;
			case HX_ATTR_A_NONPRINT: charbuf_append(b, "\x1b[36m", 5); break;
			case HX_ATTR_A_CURSOR:   charbuf_append(b, "\x1b[7;36m", 7); break;
			case HX_ATTR_A_CURSOR_P: charbuf_append(b, "\x1b[7;33m", 7); break;
			}
			attr = desired;
		}

		if (isprint(c)) {
			charbuf_append(b, &c, 1);
		} else {
			charbuf_append(b, ".", 1);
		}
	}
	charbuf_append(b, "\x1b[0m\x1b[K", 7);
}

void editor_render_contents(struct editor* e, struct charbuf* b) {
	unsigned int file_len = E_FILE_LEN(e);

	if (file_len <= 0) {
		/* Fill all data rows with dim sequential addresses + clear-to-EOL */
		int data_rows_avail = e->screen_rows - e->header_rows - 1;
		unsigned int addr = 0;
		for (int r = 1; r <= data_rows_avail; r++) {
			charbuf_appendf(b, "\x1b[2;35m%09x\x1b[0m:\x1b[0K\r\n", addr);
			addr += e->octets_per_line;
		}
		return;
	}

	char hex[ 32 + 1];
	int  hexlen = 0;

	int row_char_count = 0;

	unsigned int start_offset = e->line * e->octets_per_line;
	if (start_offset >= file_len) {
		start_offset = file_len - e->octets_per_line;
	}

	int bytes_per_screen = (e->screen_rows - e->header_rows) * e->octets_per_line;
	unsigned int end_offset = bytes_per_screen + start_offset - e->octets_per_line;
	if (end_offset > file_len) {
		end_offset = file_len;
	}

#ifdef BUSPIRATE
	/* Ensure the page covers the visible range */
	editor_ensure_page(e, start_offset, end_offset - start_offset);
#endif
	unsigned int pg = E_PAGE_OFF(e);

	unsigned int offset;

	int row = 0;
	int col = 0;
	int hex_attr = HX_ATTR_NONE;

	for (offset = start_offset; offset < end_offset; offset++) {
		unsigned char curr_byte = e->contents[offset - pg];

		if (offset % e->octets_per_line == 0) {
			charbuf_appendf(b, "\x1b[1;35m%09x\x1b[0m:", offset);
			row_char_count = 0;
			col = 0;
			row++;
			hex_attr = HX_ATTR_NONE;
		}
		col++;

		if (offset % e->grouping == 0) {
			charbuf_append(b, " ", 1);
			row_char_count++;
		}

		/* Determine desired attribute for this hex byte */
		int desired;
		if (e->cursor_y == row && e->cursor_x == col) {
			desired = HX_ATTR_CURSOR;
		} else if (isprint(curr_byte)) {
			desired = HX_ATTR_PRINT;
		} else {
			desired = HX_ATTR_NONE;
		}

		/* Emit escape only on attribute transition */
		if (desired != hex_attr) {
			if (hex_attr != HX_ATTR_NONE) charbuf_append(b, "\x1b[0m", 4);
			switch (desired) {
			case HX_ATTR_PRINT:  charbuf_append(b, "\x1b[1;34m", 7);    break;
			case HX_ATTR_CURSOR:
				/* Reverse video — reflect the byte's natural colour:
				 * printable → blue bg (reverse bold blue), else → white bg */
				if (isprint(curr_byte))
					charbuf_append(b, "\x1b[7;1;34m", 9);
				else
					charbuf_append(b, "\x1b[7m", 4);
				break;
			}
			hex_attr = desired;
		}

		hexlen = snprintf(hex, sizeof(hex), "%02x", curr_byte);
		charbuf_append(b, hex, hexlen);

		/* Cursor: reset right after the two hex digits so reverse
		 * video does not bleed into the group-separator space. */
		if (hex_attr == HX_ATTR_CURSOR) {
			charbuf_append(b, "\x1b[0m", 4);
			hex_attr = HX_ATTR_NONE;
		}

		row_char_count += 2;

		if ((offset+1) % e->octets_per_line == 0) {
			if (hex_attr != HX_ATTR_NONE) {
				charbuf_append(b, "\x1b[0m", 4);
				hex_attr = HX_ATTR_NONE;
			}
			charbuf_append(b, "  ", 2);
			int the_offset = offset + 1 - e->octets_per_line;
			editor_render_ascii(e, row, the_offset, b);
			charbuf_append(b, "\r\n", 2);
		}
	}

	unsigned int leftover = offset % e->octets_per_line;
	if (leftover > 0) {
		if (hex_attr != HX_ATTR_NONE) {
			charbuf_append(b, "\x1b[0m", 4);
			hex_attr = HX_ATTR_NONE;
		}
		int padding_size = (e->octets_per_line * 2) + (e->octets_per_line / e->grouping) - row_char_count;
		/* Stack buffer for padding — max 160 bytes (octets_per_line ≤ 64) */
		char pad[160];
		if (padding_size > (int)sizeof(pad)) padding_size = (int)sizeof(pad);
		memset(pad, ' ', padding_size);
		charbuf_append(b, pad, padding_size);
		charbuf_append(b, "  ", 2);
		editor_render_ascii(e, row, offset - leftover, b);
	}

	charbuf_append(b, "\x1b[0K", 4);

	/* ── Pad remaining rows to screen bottom with dim addresses ──
	 * Continue the address sequence past end-of-data so the gutter
	 * extends all the way down.  Uses dim magenta (\x1b[2;35m) to
	 * visually distinguish from real data rows (bold magenta). */
	{
		int data_rows_avail = e->screen_rows - e->header_rows - 1; /* -1 for status */
		unsigned int pad_addr = end_offset;
		/* Round up to next line boundary if data ended mid-row */
		if (pad_addr % e->octets_per_line != 0)
			pad_addr += e->octets_per_line - (pad_addr % e->octets_per_line);
		for (int r = row + 1; r <= data_rows_avail; r++) {
			charbuf_appendf(b, "\r\n\x1b[2;35m%09x\x1b[0m:\x1b[0K", pad_addr);
			pad_addr += e->octets_per_line;
		}
	}

#ifndef NDEBUG
#ifndef BUSPIRATE
	charbuf_appendf(b, "\x1b[0m\x1b[1;35m\x1b[1;80HRows: %d", e->screen_rows);
	charbuf_appendf(b, "\x1b[0K\x1b[2;80HOffset: %09x - %09x", start_offset, end_offset);
	charbuf_appendf(b, "\x1b[0K\x1b[3;80H(y,x)=(%d,%d)", e->cursor_y, e->cursor_x);
	unsigned int curr_offset = editor_offset_at_cursor(e);
	charbuf_appendf(b, "\x1b[0K\x1b[5;80H\x1b[0KLine: %d, cursor offset: %d (hex: %02x)", e->line, curr_offset, (unsigned char) e->contents[curr_offset]);
#endif
#endif
}

void editor_render_help(struct editor* e) {
	(void) e;
	struct charbuf* b = charbuf_create();
	clear_screen();
	charbuf_append(b, "\x1b[?25l", 6); // hide cursor
	charbuf_appendf(b, "This is hx, version %s\r\n\n", HX_VERSION);
	{
		const char help1[] =
			"Available commands:\r\n"
			"\r\n"
			"CTRL+Q  : Quit immediately without saving.\r\n"
			"CTRL+S  : Save (in place).\r\n"
			"hjkl    : Vim like cursor movement.\r\n"
			"Arrows  : Also moves the cursor around.\r\n"
			"CTRL+F  : Scroll one screen forward.\r\n"
			"CTRL+B  : Scroll one screen backward.\r\n"
			"PgDn    : Same as CTRL+F\r\n"
			"PgUp    : Same as CTRL+B\r\n"
			"w       : Skip one group of bytes to the right.\r\n"
			"b       : Skip one group of bytes to the left.\r\n"
			"gg      : Move to start of file.\r\n"
			"G       : Move to end of file.\r\n"
			"x / DEL : Delete byte at cursor position.\r\n"
			"/       : Start search input.\r\n"
			"n       : Search for next occurrence.\r\n"
			"N       : Search for previous occurrence.\r\n"
			"u       : Undo the last action.\r\n"
			"CTRL+R  : Redo the last undone action.\r\n"
			"\r\n";
		charbuf_append(b, help1, sizeof(help1) - 1);
	}
	{
		const char help2[] =
			"a       : Append mode. Appends a byte after the current cursor position.\r\n"
			"A       : Append mode. Appends the literal typed keys (except ESC).\r\n"
			"i       : Insert mode. Inserts a byte at the current cursor position.\r\n"
			"I       : Insert mode. Inserts the literal typed keys (except ESC).\r\n"
			"r       : Replace mode. Replaces the byte at the current cursor position.\r\n"
			"R       : Replace mode. Replaces the literal typed keys (except ESC).\r\n"
			":       : Command mode. Commands can be typed and executed.\r\n"
			"ESC     : Return to normal mode.\r\n"
			"]       : Increment byte at cursor position with 1.\r\n"
			"[       : Decrement byte at cursor position with 1.\r\n"
			"End     : Move cursor to end of the offset line.\r\n"
			"Home    : Move cursor to the beginning of the offset line.\r\n"
			"\r\n";
		charbuf_append(b, help2, sizeof(help2) - 1);
	}
	charbuf_append(b,
		"Press any key to exit help.\r\n", 28);

	charbuf_draw(b);
	charbuf_free(b);
	read_key();
	clear_screen();
}



void editor_render_ruler(struct editor* e, struct charbuf* b) {
	unsigned int file_len = E_FILE_LEN(e);
	if (file_len <= 0) {
		return;
	}

	char rulermsg[80];
	char buf[20];

	unsigned int offset_at_cursor = editor_offset_at_cursor(e);
	unsigned int pg = E_PAGE_OFF(e);
	unsigned char val = 0;
	/* Read byte under cursor — make sure it's within the loaded page */
	if (offset_at_cursor >= pg && offset_at_cursor < pg + e->content_length) {
		val = e->contents[offset_at_cursor - pg];
	}
	int percentage = (float)(offset_at_cursor + 1) / (float)file_len * 100;

	int rmbw = snprintf(rulermsg, sizeof(rulermsg),
		"0x%09x,%d (%02x)  %d%%",
		offset_at_cursor, offset_at_cursor, val, percentage);
	if (rmbw < 0) {
		fprintf(stderr, "Could not create ruler string!");
		return;
	}

	int cpbw = snprintf(buf, sizeof(buf), "\x1b[0m\x1b[%d;%dH", e->screen_rows, e->screen_cols - rmbw);
	if (cpbw < 0) {
		fprintf(stderr, "Could not create cursor position string!");
		return;
	}

	charbuf_append(b, buf, cpbw);
	charbuf_append(b, rulermsg, rmbw);
}


void editor_render_status(struct editor* e, struct charbuf* b) {
	charbuf_appendf(b, "\x1b[%d;0H", e->screen_rows);

	switch (e->status_severity) {
	case STATUS_INFO:    charbuf_append(b, "\x1b[0;30;47m", 10); break;
	case STATUS_WARNING: charbuf_append(b, "\x1b[0;30;43m", 10); break;
	case STATUS_ERROR:   charbuf_append(b, "\x1b[1;37;41m", 10); break;
	}

#ifdef BUSPIRATE
	if (e->paged) {
		unsigned int cur_page = e->page_offset / e->page_size;
		charbuf_appendf(b, "\"%s\" (%u bytes) [READ-ONLY, PAGE %u]",
			e->filename, e->file_size, cur_page);
	} else
#endif
	{
		int maxchars = strlen(e->status_message);
		if (e->screen_cols <= maxchars) {
			maxchars = e->screen_cols;
		}
		charbuf_append(b, e->status_message, maxchars);
	}
	charbuf_append(b, "\x1b[0m\x1b[0K", 8);
}


void editor_render_header(struct editor* e, struct charbuf* b) {
	/* Offset gutter — same width as the "%09x:" in content rows */
	charbuf_append(b, "\x1b[0;37m", 7);
	charbuf_append(b, "         |", 10);

	/* Hex column headers: single hex digit per byte, spaced to align
	 * with the grouped hex pairs in content rows.
	 * Content row pattern (grouping=2): " XXYY" per pair
	 * Header pattern:                   " X Y " per pair */
	for (int i = 0; i < e->octets_per_line; i++) {
		if (i % e->grouping == 0) {
			charbuf_append(b, " ", 1);
		}
		char hdr[3];
		int hlen = snprintf(hdr, sizeof(hdr), "%X", i & 0xF);
		charbuf_append(b, hdr, hlen);
		/* After each byte's single hex digit, pad with a space
		 * to fill the second character of its "xx" column */
		charbuf_append(b, " ", 1);
	}

	/* Gap + ASCII column header */
	charbuf_append(b, " |", 2);
	for (int i = 0; i < e->octets_per_line; i++) {
		char hdr[2];
		int hlen = snprintf(hdr, sizeof(hdr), "%X", i & 0xF);
		charbuf_append(b, hdr, hlen);
	}

	charbuf_append(b, "\x1b[0m\x1b[K\r\n", 10);

	/* Separator line */
	charbuf_append(b, "---------+", 10);
	for (int i = 0; i < e->octets_per_line; i++) {
		if (i % e->grouping == 0) {
			charbuf_append(b, "-", 1);
		}
		charbuf_append(b, "--", 2);
	}
	charbuf_append(b, "-+", 2);
	for (int i = 0; i < e->octets_per_line; i++) {
		charbuf_append(b, "-", 1);
	}
	charbuf_append(b, "\x1b[K\r\n", 5);
}


void editor_refresh_screen(struct editor* e) {
	struct charbuf* b = charbuf_create();

	charbuf_append(b, "\x1b[?25l", 6);
	if (e->header_rows > 0) {
		/* Position after externally-owned rows (menu bar, config bar, etc.);
		 * the editor draws its own column headers + separator from here. */
		charbuf_appendf(b, "\x1b[%d;1H", e->header_rows - 1);
	} else {
		charbuf_append(b, "\x1b[H", 3);
	}

	if (e->mode &
			(MODE_REPLACE |
			 MODE_NORMAL |
			 MODE_APPEND |
			 MODE_APPEND_ASCII |
			 MODE_REPLACE_ASCII |
			 MODE_INSERT |
			 MODE_INSERT_ASCII)) {

		editor_render_header(e, b);
		editor_render_contents(e, b);
		editor_render_status(e, b);
		editor_render_ruler(e, b);
	} else if (e->mode & MODE_COMMAND) {
		charbuf_appendf(b,
			"\x1b[0m"
			"\x1b[?25h"
			"\x1b[%d;1H"
			"\x1b[2K:",
			e->screen_rows);
		charbuf_append(b, e->inputbuffer, e->inputbuffer_index);
	} else if (e->mode & MODE_SEARCH) {
		charbuf_appendf(b,
			"\x1b[0m"
			"\x1b[?25h"
			"\x1b[%d;1H"
			"\x1b[2K/",
			e->screen_rows);
		charbuf_append(b, e->inputbuffer, e->inputbuffer_index);
	}

	charbuf_draw(b);
	charbuf_free(b);
}


void editor_insert_byte(struct editor* e, char x, bool after) {
	int offset = editor_offset_at_cursor(e);
	editor_insert_byte_at_offset(e, offset, x, after);

	if (e->undo_list) {
		if (after) {
			action_list_add(e->undo_list, ACTION_APPEND, offset, x);
		} else {
			action_list_add(e->undo_list, ACTION_INSERT, offset, x);
		}
	}
}

void editor_insert_byte_at_offset(struct editor* e, unsigned int offset, char x, bool after) {
	e->contents = realloc(e->contents, e->content_length + 1);

	if (after && e->content_length) {
		offset++;
	}
	memmove(e->contents + offset + 1, e->contents + offset, e->content_length - offset);

	e->contents[offset] = x;
	e->content_length++;
	e->dirty = true;
}


void editor_replace_byte(struct editor* e, char x) {
	unsigned int offset = editor_offset_at_cursor(e);
	unsigned char prev = e->contents[offset];
	e->contents[offset] = x;
	editor_move_cursor(e, KEY_RIGHT, 1);
	editor_statusmessage(e, STATUS_INFO, "Replaced byte at offset %09x with %02x", offset, (unsigned char) x);
	e->dirty = true;

	if (e->undo_list) action_list_add(e->undo_list, ACTION_REPLACE, offset, prev);
}

void editor_process_command(struct editor* e, const char* cmd) {
	unsigned int file_len = E_FILE_LEN(e);

	// Command: go to base 10 offset
	bool b = is_pos_num(cmd);
	if (b) {
		int offset = str2int(cmd, 0, file_len, file_len - 1);
		editor_scroll_to_offset(e, offset);
		editor_statusmessage(e, STATUS_INFO, "Positioned to offset 0x%09x (%d)", offset, offset);
		return;
	}

	// Command: go to hex offset
	if (cmd[0] == '0' && cmd[1] == 'x') {
		const char* ptr = &cmd[2];
		if (!is_hex(ptr)) {
			editor_statusmessage(e, STATUS_ERROR, "Error: %s is not valid base 16", ptr);
			return;
		}

		int offset = hex2int(ptr);
		editor_scroll_to_offset(e, offset);
		editor_statusmessage(e, STATUS_INFO, "Positioned to offset 0x%09x (%d)", offset, offset);
		return;
	}

	if (strncmp(cmd, "w", INPUT_BUF_SIZE) == 0) {
#ifdef BUSPIRATE
		if (e->paged) {
			editor_statusmessage(e, STATUS_WARNING, "Read-only paged view — cannot save");
			return;
		}
#endif
		editor_writefile(e);
		return;
	}

	if (strncmp(cmd, "q", INPUT_BUF_SIZE) == 0) {
		if (e->dirty) {
			editor_statusmessage(e, STATUS_ERROR, "No write since last change (add ! to override)", cmd);
			return;
		} else {
			e->quit_requested = true;
			return;
		}
	}

	if (strncmp(cmd, "wq", INPUT_BUF_SIZE) == 0 || strncmp(cmd, "wq!", INPUT_BUF_SIZE) == 0) {
		editor_writefile(e);
		e->quit_requested = true;
		return;
	}

	if (strncmp(cmd, "q!", INPUT_BUF_SIZE) == 0) {
		e->quit_requested = true;
		return;
	}

	if (strncmp(cmd, "help", INPUT_BUF_SIZE) == 0) {
		editor_render_help(e);
		return;
	}

	// Check if we want to set an option at runtime.
	if (strncmp(cmd, "set", 3) == 0) {
		char setcmd[INPUT_BUF_SIZE] = {0};
		int setval = 0;
		int items_read = sscanf(cmd, "set %[a-z]=%d", setcmd, &setval);

		if (items_read != 2) {
			editor_statusmessage(e, STATUS_ERROR, "set command format: `set cmd=num`");
			return;
		}

		if (strcmp(setcmd, "octets") == 0 || strcmp(setcmd, "o") == 0) {
			int octets = clampi(setval, 16, 64);

			clear_screen();
			int offset = editor_offset_at_cursor(e);
			e->octets_per_line = octets;
			editor_scroll_to_offset(e, offset);

			editor_statusmessage(e, STATUS_INFO, "Octets per line set to %d", octets);

			return;
		}

		if (strcmp(setcmd, "grouping") == 0 || strcmp(setcmd, "g") == 0) {
			int grouping = clampi(setval, 4, 16);
			clear_screen();
			e->grouping = grouping;

			editor_statusmessage(e, STATUS_INFO, "Byte grouping set to %d", grouping);
			return;
		}

		editor_statusmessage(e, STATUS_ERROR, "Unknown option: %s", setcmd);
		return;
	}

	editor_statusmessage(e, STATUS_ERROR, "Command not found: %s", cmd);
}

void editor_process_search(struct editor* e, const char* str, enum search_direction dir) {
	if (strncmp(str, "", INPUT_BUF_SIZE) == 0) {
		strncpy(e->searchstr, str, INPUT_BUF_SIZE);
		return;
	}

	if (strncmp(str, e->searchstr, INPUT_BUF_SIZE) != 0) {
		strncpy(e->searchstr, str, INPUT_BUF_SIZE);
	}

	if (dir == SEARCH_BACKWARD && editor_offset_at_cursor(e) == 0) {
		editor_statusmessage(e, STATUS_INFO, "Already at start of the file");
		return;
	}

	struct charbuf *parsedstr = charbuf_create();
	const char* parse_err;
	int parse_errno = editor_parse_search_string(str, parsedstr, &parse_err);
	switch (parse_errno) {
	case PARSE_INCOMPLETE_BACKSLASH:
		editor_statusmessage(e, STATUS_ERROR,
				     "Nothing follows '\\' in search"
				     " string: %s", str);
		break;
	case PARSE_INCOMPLETE_HEX:
		editor_statusmessage(e, STATUS_ERROR,
				     "Incomplete hex value at end"
				     " of search string: %s", str);
		break;
	case PARSE_INVALID_HEX:
		editor_statusmessage(e, STATUS_ERROR,
				     "Invalid hex value (\\x%c%c)"
				     " in search string: %s",
				     *parse_err, *(parse_err + 1), str);
		break;
	case PARSE_INVALID_ESCAPE:
		editor_statusmessage(e, STATUS_ERROR,
				     "Invalid character after \\ (%c)"
				     " in search string: %s",
				     *parse_err, str);
		break;
	case PARSE_SUCCESS:
		break;
	}

	if (parse_errno != PARSE_SUCCESS) {
		charbuf_free(parsedstr);
		return;
	}

	unsigned int current_offset = editor_offset_at_cursor(e);
	bool found = false;
	if (dir == SEARCH_FORWARD) {
		current_offset++;
		for (; current_offset < e->content_length; current_offset++) {
			if (memcmp(e->contents + current_offset,
				   parsedstr->contents, parsedstr->len) == 0) {
				editor_statusmessage(e, STATUS_INFO, "");
				editor_scroll_to_offset(e, current_offset);
				found = true;
				break;
			}
		}
	} else if (dir == SEARCH_BACKWARD) {
		current_offset--;

		for (; current_offset-- != 0; ) {
			if (memcmp(e->contents + current_offset,
				   parsedstr->contents, parsedstr->len) == 0) {
				editor_statusmessage(e, STATUS_INFO, "");
				editor_scroll_to_offset(e, current_offset);
				found = true;
				break;
			}
		}
	}

	charbuf_free(parsedstr);
	if (!found) editor_statusmessage(e, STATUS_WARNING,
					 "String not found: '%s'", str);
}

int editor_parse_search_string(const char* inputstr, struct charbuf* parsedstr,
			       const char** err_info) {
	char hex[3] = {'\0'};
	*err_info = inputstr;

	while (*inputstr != '\0') {
		if (*inputstr == '\\') {
			++inputstr;
			switch (*(inputstr)) {
			case '\0':
				return PARSE_INCOMPLETE_BACKSLASH;
			case '\\':
				charbuf_append(parsedstr, "\\", 1);
				++inputstr;
				break;
			case 'x':
				++inputstr;

				if (*inputstr == '\0'
				    || *(inputstr + 1) == '\0') {
					return PARSE_INCOMPLETE_HEX;
				}

				if (!isxdigit(*inputstr)
				    || !isxdigit(*(inputstr + 1))) {
					*err_info = inputstr;
					return PARSE_INVALID_HEX;
				}

				memcpy(hex, inputstr, 2);
				char bin = hex2bin(hex);
				charbuf_append(parsedstr, &bin, 1);

				inputstr += 2;
				break;
			default:
				*err_info = inputstr;
				return PARSE_INVALID_ESCAPE;
			}
		} else {
			charbuf_append(parsedstr, inputstr, 1);
			++inputstr;
		}
	}

	return PARSE_SUCCESS;
}

int editor_read_hex_input(struct editor* e, char* out) {
	static int  hexstr_idx = 0;
	static char hexstr[2 + 1];

	int next = read_key();

	if (next == KEY_ESC) {
		editor_setmode(e, MODE_NORMAL);
		memset(hexstr, '\0', 3);
		hexstr_idx = 0;
		return -1;
	}

	if (!isprint(next)) {
		editor_statusmessage(e, STATUS_ERROR, "Error: unprintable character (%02x)", next);
		return -1;
	}
	if (!isxdigit(next)) {
		editor_statusmessage(e, STATUS_ERROR, "Error: '%c' (%02x) is not valid hex", next, next);
		return -1;
	}

	hexstr[hexstr_idx++] = next;

	if (hexstr_idx >= 2) {
		*out = hex2bin(hexstr);
		memset(hexstr, '\0', 3);
		hexstr_idx = 0;
		return 0;
	}

	return -1;
}


int editor_read_string(struct editor* e, char* dst, int len) {
	int c = read_key();
	if (c == KEY_ENTER || c == KEY_ESC) {
		editor_setmode(e, MODE_NORMAL);
		strncpy(dst, e->inputbuffer, len);
		e->inputbuffer_index = 0;
		memset(e->inputbuffer,  '\0', sizeof(e->inputbuffer));
		return c;
	}

	if (c == KEY_BACKSPACE && e->inputbuffer_index > 0) {
		e->inputbuffer_index--;
		e->inputbuffer[e->inputbuffer_index] = '\0';
		return c;
	}

	if ((size_t) e->inputbuffer_index >= sizeof(e->inputbuffer) - 1) {
		return c;
	}

	if (c == KEY_BACKSPACE && e->inputbuffer_index == 0) {
		editor_setmode(e, MODE_NORMAL);
		return c;
	}

	if (!isprint(c)) {
		return c;
	}

	e->inputbuffer[e->inputbuffer_index++] = c;
	return c;
}


void editor_process_keypress(struct editor* e) {
	if (e->mode & (MODE_INSERT | MODE_APPEND)) {
		char out = 0;
		if (editor_read_hex_input(e, &out) != -1) {
			editor_insert_byte(e, out, e->mode & MODE_APPEND);
			editor_move_cursor(e, KEY_RIGHT, 1);
		}
		return;
	}

	if (e->mode & (MODE_INSERT_ASCII | MODE_APPEND_ASCII)) {
		char c = read_key();
		if (c == KEY_ESC) {
			editor_setmode(e, MODE_NORMAL);
			return;
		}
		editor_insert_byte(e, c, e->mode & MODE_APPEND_ASCII);
		editor_move_cursor(e, KEY_RIGHT, 1);
		return;
	}

	if (e->mode & MODE_REPLACE_ASCII) {
		char c = read_key();
		if (c == KEY_ESC) {
			editor_setmode(e, MODE_NORMAL);
			return;
		}

		if (e->content_length > 0) {
			editor_replace_byte(e, c);
		} else {
			editor_statusmessage(e, STATUS_ERROR, "File is empty, nothing to replace");
		}
		return;
	}

	if (e->mode & MODE_REPLACE) {
		char out = 0;
		if (e->content_length > 0) {
			if (editor_read_hex_input(e, &out) != -1) {
				editor_replace_byte(e, out);
			}
			return;
		} else {
			editor_statusmessage(e, STATUS_ERROR, "File is empty, nothing to replace");
		}
	}

	if (e->mode & MODE_COMMAND) {
		char cmd[INPUT_BUF_SIZE];
		int c = editor_read_string(e, cmd, INPUT_BUF_SIZE);
		if (c == KEY_ENTER && strlen(cmd) > 0) {
			editor_process_command(e, cmd);
		}
		return;
	}

	if (e->mode & MODE_SEARCH) {
		char search[INPUT_BUF_SIZE];
		int c = editor_read_string(e, search, INPUT_BUF_SIZE);
		if (c == KEY_ENTER && strlen(search) > 0) {
			editor_process_search(e, search, SEARCH_FORWARD);
		}
		return;
	}


	int c = read_key();
	if (c == -1) {
		return;
	}

	switch (c) {
	case KEY_ESC:    if (!(e->mode & MODE_NORMAL)) editor_setmode(e, MODE_NORMAL); return;
	case KEY_CTRL_Q: e->quit_requested = true; return;
	case KEY_CTRL_S:
#ifdef BUSPIRATE
		if (e->paged) {
			editor_statusmessage(e, STATUS_WARNING, "Read-only paged view");
			return;
		}
#endif
		editor_writefile(e); return;
	}

	if (e->mode & MODE_NORMAL) {
#ifdef BUSPIRATE
		/* ── Paged mode: block all editing keys ── */
		if (e->paged) {
			switch (c) {
			case ']': case '[':
			case KEY_DEL: case 'x':
			case 'a': case 'A':
			case 'i': case 'I':
			case 'r': case 'R':
			case 'u': case KEY_CTRL_R:
			case '/': case 'n': case 'N':
				editor_statusmessage(e, STATUS_WARNING, "Read-only paged view");
				return;
			}
		}
#endif
		unsigned int file_len = E_FILE_LEN(e);

		switch (c) {
		case KEY_UP:
		case KEY_DOWN:
		case KEY_RIGHT:
		case KEY_LEFT:
			editor_move_cursor(e, c, 1); break;

		case 'h': editor_move_cursor(e, KEY_LEFT,  1); break;
		case 'j': editor_move_cursor(e, KEY_DOWN,  1); break;
		case 'k': editor_move_cursor(e, KEY_UP,    1); break;
		case 'l': editor_move_cursor(e, KEY_RIGHT, 1); break;
		case ']': editor_increment_byte(e, 1); break;
		case '[': editor_increment_byte(e, -1); break;
		case KEY_DEL:
		case 'x': editor_delete_char_at_cursor(e); break;
		case 'n': editor_process_search(e, e->searchstr, SEARCH_FORWARD); break;
		case 'N': editor_process_search(e, e->searchstr, SEARCH_BACKWARD); break;

		case 'a': editor_setmode(e, MODE_APPEND);       return;
		case 'A': editor_setmode(e, MODE_APPEND_ASCII); return;
		case 'i': editor_setmode(e, MODE_INSERT);       return;
		case 'I': editor_setmode(e, MODE_INSERT_ASCII); return;
		case 'r': editor_setmode(e, MODE_REPLACE);      return;
		case 'R': editor_setmode(e, MODE_REPLACE_ASCII);return;
		case ':': editor_setmode(e, MODE_COMMAND);      return;
		case '/': editor_setmode(e, MODE_SEARCH);       return;

		case 'u':          editor_undo(e); return;
		case KEY_CTRL_R :  editor_redo(e); return;

		case 'b': editor_move_cursor(e, KEY_LEFT, e->grouping); break;
		case 'w': editor_move_cursor(e, KEY_RIGHT, e->grouping); break;
		case 'G':
			editor_scroll(e, file_len);
			if (file_len > 0)
				editor_cursor_at_offset(e, file_len - 1, &e->cursor_x, &e->cursor_y);
			break;
		case 'g':
			c = read_key();
			if (c == 'g') {
				e->line = 0;
				editor_cursor_at_offset(e, 0, &e->cursor_x, &e->cursor_y);
			}
			break;

		case KEY_HOME: e->cursor_x = 1; return;
		case KEY_END:  editor_move_cursor(e, KEY_RIGHT, e->octets_per_line - e->cursor_x); return;

		case KEY_CTRL_U:
		case KEY_PAGEUP:   editor_scroll(e, -(e->screen_rows - e->header_rows) + 2); return;

		case KEY_CTRL_D:
		case KEY_PAGEDOWN: editor_scroll(e, e->screen_rows - e->header_rows - 2); return;

#ifdef BUSPIRATE
		case KEY_F10: e->menu_pending = true; return;
#endif
		}
	}
}

void editor_undo(struct editor* e) {
	if (!e->undo_list) {
		editor_statusmessage(e, STATUS_INFO, "Undo disabled");
		return;
	}
	struct action* last_action = e->undo_list->curr;

	if (e->undo_list->curr_status == AFTER_TAIL) {
		action_list_move(e->undo_list, -1);
	}

	if (e->undo_list->curr_status != NODE) {
		editor_statusmessage(e, STATUS_INFO, "No action to undo");
		return;
	}

	char old_contents = e->contents[last_action->offset];
	switch (last_action->act) {
	case ACTION_APPEND:
		editor_delete_char_at_offset(e, last_action->offset+1);
		break;
	case ACTION_DELETE:
		editor_insert_byte_at_offset(e, last_action->offset, last_action->c, false);
		break;
	case ACTION_REPLACE:
		e->contents[last_action->offset] = last_action->c;
		last_action->c = old_contents;
		break;
	case ACTION_INSERT:
		editor_delete_char_at_offset(e, last_action->offset);
		break;
	}

	editor_scroll_to_offset(e, last_action->offset);
	action_list_move(e->undo_list, -1);

	editor_statusmessage(e, STATUS_INFO,
		"Reverted '%s' at offset %d to byte '%02x' (%d left)",
			action_type_name(last_action->act),
			last_action->offset,
			last_action->c,
			action_list_curr_pos(e->undo_list));
}

void editor_redo(struct editor* e) {
	if (!e->undo_list) {
		editor_statusmessage(e, STATUS_INFO, "Redo disabled");
		return;
	}
	if (e->undo_list->curr_status == AFTER_TAIL
	    || e->undo_list->curr_status == NOTHING) {
		editor_statusmessage(e, STATUS_INFO, "No action to redo");
		return;
	}

	struct action* next_action = NULL;

	if (e->undo_list->curr_status == BEFORE_HEAD) {
		next_action = e->undo_list->head;
	} else {
		next_action = e->undo_list->curr->next;
	}

	if (next_action == NULL) {
		editor_statusmessage(e, STATUS_INFO, "No action to redo");
		return;
	}

	char old_contents = e->contents[next_action->offset];
	switch (next_action->act) {
	case ACTION_APPEND:
		editor_insert_byte_at_offset(e, next_action->offset, next_action->c, true);
		break;
	case ACTION_DELETE:
		editor_delete_char_at_offset(e, next_action->offset);
		break;
	case ACTION_REPLACE:
		e->contents[next_action->offset] = next_action->c;
		next_action->c = old_contents;
		break;
	case ACTION_INSERT:
		editor_insert_byte_at_offset(e, next_action->offset, next_action->c, false);
		break;
	}

	editor_scroll_to_offset(e, next_action->offset);
	action_list_move(e->undo_list, 1);

	editor_statusmessage(e, STATUS_INFO,
		"Redone '%s' at offset %d to byte '%02x' (%d left)",
			action_type_name(next_action->act),
			next_action->offset,
			next_action->c,
			action_list_size(e->undo_list)
			- action_list_curr_pos(e->undo_list));
}

/*
 * Initializes editor struct with some default values.
 */
struct editor* editor_init() {
	struct editor* e = malloc(sizeof(struct editor));
	if (e == NULL) {
		perror("Cannot allocate memory for editor");
		abort();
	}

	e->octets_per_line = 16;
	e->grouping = 2;

	e->line = 0;
	e->cursor_x = 1;
	e->cursor_y = 1;
	e->filename = NULL;
	e->contents = NULL;
	e->content_length = 0;
	e->dirty = false;

	memset(e->status_message, '\0', sizeof(e->status_message));

	e->mode = MODE_NORMAL;

	memset(e->inputbuffer, '\0', sizeof(e->inputbuffer));
	e->inputbuffer_index = 0;

	memset(e->searchstr, '\0', sizeof(e->searchstr));

	get_window_size(&(e->screen_rows), &(e->screen_cols));
	e->header_rows = 0;

	e->quit_requested = false;

#ifdef BUSPIRATE
	e->undo_list = NULL;  /* undo disabled — zero-alloc debug build */
	e->menu_pending = false;
	e->paged = false;
	e->file_size = 0;
	e->page_offset = 0;
	e->page_size = HX_PAGE_SIZE;
	e->paged_path = NULL;
#else
	e->undo_list = action_list_init();
#endif

	return e;
}

void editor_free(struct editor* e) {
	if (e->undo_list) action_list_free(e->undo_list);
#ifdef BUSPIRATE
	if (e->paged_path) free(e->paged_path);
#endif
	free(e->filename);
	free(e->contents);
	free(e);
}

/* ======================================================================
 * Bus Pirate entry points — called from hexedit_handler() via setjmp
 * ====================================================================== */
#ifdef BUSPIRATE

static struct editor* g_hx_editor = NULL;

/* ── Menu definitions for the hex editor ──────────────────────────── */

/* Action IDs — defined in editor.h, used by vt100_menu_run() */

static const vt100_menu_item_t hx_file_items[] = {
	{ "Save",          "^S",  HX_ACT_SAVE,       0 },
	{ NULL,             NULL,   0,                  MENU_ITEM_SEPARATOR },
	{ "Quit",          "^Q",  HX_ACT_QUIT,       0 },
	{ "Quit (no save)", NULL,  HX_ACT_QUIT_NOSAVE,0 },
	{ NULL, NULL, 0, 0 }  /* sentinel */
};

static const vt100_menu_item_t hx_edit_items[] = {
	{ "Undo",          "u",   HX_ACT_UNDO,       0 },
	{ "Redo",          "^R",  HX_ACT_REDO,       0 },
	{ NULL,             NULL,   0,                  MENU_ITEM_SEPARATOR },
	{ "Delete byte",   "x",   HX_ACT_DEL,        0 },
	{ "Increment",     "]",   HX_ACT_INC,        0 },
	{ "Decrement",     "[",   HX_ACT_DEC,        0 },
	{ NULL,             NULL,   0,                  MENU_ITEM_SEPARATOR },
	{ "Insert hex",    "i",   HX_ACT_MODE_INS,   0 },
	{ "Append hex",    "a",   HX_ACT_MODE_APP,   0 },
	{ "Replace hex",   "r",   HX_ACT_MODE_REP,   0 },
	{ "Insert ASCII",  "I",   HX_ACT_MODE_INSA,  0 },
	{ "Append ASCII",  "A",   HX_ACT_MODE_APPA,  0 },
	{ "Replace ASCII", "R",   HX_ACT_MODE_REPA,  0 },
	{ NULL, NULL, 0, 0 }
};

static const vt100_menu_item_t hx_search_items[] = {
	{ "Search",        "/",   HX_ACT_SEARCH,     0 },
	{ "Next match",    "n",   HX_ACT_NEXT,       0 },
	{ "Prev match",    "N",   HX_ACT_PREV,       0 },
	{ NULL,             NULL,   0,                  MENU_ITEM_SEPARATOR },
	{ "Go to offset",  ":",   HX_ACT_GOTO,       0 },
	{ "Go to start",   "gg",  HX_ACT_GOTO_TOP,   0 },
	{ "Go to end",     "G",   HX_ACT_GOTO_END,   0 },
	{ NULL, NULL, 0, 0 }
};

static const vt100_menu_item_t hx_help_items[] = {
	{ "Help",          ":help", HX_ACT_HELP,     0 },
	{ NULL, NULL, 0, 0 }
};

static const vt100_menu_def_t hx_menus[] = {
	{ "File",   hx_file_items,   4 },
	{ "Edit",   hx_edit_items,   12 },
	{ "Search", hx_search_items, 7 },
	{ "Help",   hx_help_items,   1 },
};
#define HX_MENU_COUNT 4

/* I/O callbacks for the menu framework — delegate to hx_compat shims */
static int hx_menu_read_key(void) {
	return read_key();
}

static int hx_menu_write(int fd, const void* buf, int count) {
	(void)fd;
	return (int)hx_io_write(1, buf, (size_t)count);
}

static void hx_menu_repaint(void) {
	editor_refresh_screen(g_hx_editor);
}

/**
 * Process a menu action returned by vt100_menu_run().
 * Dispatches to the same editor functions as the keyboard shortcuts.
 */
void hx_menu_dispatch(struct editor* e, int action) {
	/* Block editing/search actions in paged mode */
	if (e->paged) {
		switch (action) {
		case HX_ACT_SAVE:
		case HX_ACT_UNDO: case HX_ACT_REDO:
		case HX_ACT_DEL: case HX_ACT_INC: case HX_ACT_DEC:
		case HX_ACT_MODE_INS: case HX_ACT_MODE_APP: case HX_ACT_MODE_REP:
		case HX_ACT_MODE_INSA: case HX_ACT_MODE_APPA: case HX_ACT_MODE_REPA:
		case HX_ACT_SEARCH: case HX_ACT_NEXT: case HX_ACT_PREV:
			editor_statusmessage(e, STATUS_WARNING, "Read-only paged view");
			return;
		}
	}

	switch (action) {
	case HX_ACT_SAVE:       editor_writefile(e); break;
	case HX_ACT_QUIT:
		if (e->dirty) {
			editor_statusmessage(e, STATUS_ERROR,
				"Unsaved changes! Use File > Quit (no save) to discard");
		} else {
			e->quit_requested = true;
		}
		break;
	case HX_ACT_QUIT_NOSAVE: e->quit_requested = true; break;
	case HX_ACT_UNDO:        editor_undo(e); break;
	case HX_ACT_REDO:        editor_redo(e); break;
	case HX_ACT_DEL:         editor_delete_char_at_cursor(e); break;
	case HX_ACT_INC:         editor_increment_byte(e, 1); break;
	case HX_ACT_DEC:         editor_increment_byte(e, -1); break;
	case HX_ACT_MODE_INS:    editor_setmode(e, MODE_INSERT); break;
	case HX_ACT_MODE_APP:    editor_setmode(e, MODE_APPEND); break;
	case HX_ACT_MODE_REP:    editor_setmode(e, MODE_REPLACE); break;
	case HX_ACT_MODE_INSA:   editor_setmode(e, MODE_INSERT_ASCII); break;
	case HX_ACT_MODE_APPA:   editor_setmode(e, MODE_APPEND_ASCII); break;
	case HX_ACT_MODE_REPA:   editor_setmode(e, MODE_REPLACE_ASCII); break;
	case HX_ACT_SEARCH:      editor_setmode(e, MODE_SEARCH); break;
	case HX_ACT_NEXT:        editor_process_search(e, e->searchstr, SEARCH_FORWARD); break;
	case HX_ACT_PREV:        editor_process_search(e, e->searchstr, SEARCH_BACKWARD); break;
	case HX_ACT_GOTO:        editor_setmode(e, MODE_COMMAND); break;
	case HX_ACT_GOTO_TOP:
		e->line = 0;
		editor_cursor_at_offset(e, 0, &e->cursor_x, &e->cursor_y);
		break;
	case HX_ACT_GOTO_END: {
		unsigned int fl = E_FILE_LEN(e);
		editor_scroll(e, fl);
		if (fl > 0)
			editor_cursor_at_offset(e, fl - 1, &e->cursor_x, &e->cursor_y);
		break;
	}
	case HX_ACT_HELP:        editor_render_help(e); break;
	}
}

int hx_run(const char* filename) {
	hx_vt100_keys_init();
	g_hx_editor = editor_init();
	g_hx_editor->header_rows = 3; /* menu bar + column headers + separator */
	editor_openfile(g_hx_editor, filename);
	clear_screen();

	/* Initialise the menu system */
	vt100_menu_state_t menu_state;
	vt100_menu_init(&menu_state, hx_menus, HX_MENU_COUNT,
		1, /* bar_row: top of screen */
		(uint8_t)g_hx_editor->screen_cols,
		(uint8_t)g_hx_editor->screen_rows,
		hx_menu_read_key,
		hx_menu_write);
	/* Key codes: hx's enum values now match vt100_keys.h via the
	 * #ifdef BUSPIRATE redefinition in util.h, so the menu defaults
	 * are correct and no overrides are needed. */
	menu_state.repaint   = hx_menu_repaint;

	/* Snapshot fields used to detect "nothing changed" after key processing.
	 * When the cursor is clamped at the top or bottom of the file,
	 * repeated arrow keys don't alter any visible state — skipping the
	 * redraw in that case avoids the VT100 corruption that appears
	 * during fast auto-repeat. */
	int prev_cx = 0, prev_cy = 0, prev_line = 0;
	unsigned int prev_len = 0;
	bool prev_dirty = false;
	enum editor_mode prev_mode = MODE_NORMAL;

	while (1) {
		tx_fifo_wait_drain();
		editor_refresh_screen(g_hx_editor);

		/* Draw the passive menu bar hint ("F10=Menu") */
		vt100_menu_draw_bar(&menu_state);

		/* hx uses reverse-video for its byte cursor, not a terminal
		 * cursor.  draw_bar restores + shows the cursor, so hide it
		 * again to avoid a stray blink near the status ruler. */
		hx_io_write(1, "\x1b[?25l", 6);

		/* Save state before processing keys */
		prev_cx    = g_hx_editor->cursor_x;
		prev_cy    = g_hx_editor->cursor_y;
		prev_line  = g_hx_editor->line;
		prev_len   = g_hx_editor->content_length;
		prev_dirty = g_hx_editor->dirty;
		prev_mode  = g_hx_editor->mode;

		do {
			editor_process_keypress(g_hx_editor);
			if (g_hx_editor->quit_requested) break;

			/* Check if F10 was pressed (flagged in keypress handler) */
			if (g_hx_editor->menu_pending) {
				g_hx_editor->menu_pending = false;
				int action = vt100_menu_run(&menu_state);
				if (action > 0) {
					hx_menu_dispatch(g_hx_editor, action);
				} else if (action == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
					read_key_unget(menu_state.unhandled_key);
				}
				clear_screen();
				prev_line = -1;  /* invalidate so skip-redraw loop is bypassed */
				break;  /* force full redraw */
			}
		} while (spsc_queue_level(&rx_fifo) > 0);
		if (g_hx_editor->quit_requested) break;

		/* If nothing visually changed, skip the next redraw and
		 * just wait for a key that actually moves something. */
		while (g_hx_editor->cursor_x      == prev_cx   &&
		       g_hx_editor->cursor_y      == prev_cy   &&
		       g_hx_editor->line          == prev_line  &&
		       g_hx_editor->content_length == prev_len  &&
		       g_hx_editor->dirty         == prev_dirty &&
		       g_hx_editor->mode          == prev_mode) {
			editor_process_keypress(g_hx_editor);
			if (g_hx_editor->quit_requested) break;

			if (g_hx_editor->menu_pending) {
				g_hx_editor->menu_pending = false;
				int action = vt100_menu_run(&menu_state);
				if (action > 0) {
					hx_menu_dispatch(g_hx_editor, action);
				} else if (action == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
					read_key_unget(menu_state.unhandled_key);
				}
				clear_screen();
				break;  /* force full redraw */
			}

			/* Batch any further repeats that also land on the boundary */
			while (spsc_queue_level(&rx_fifo) > 0) {
				editor_process_keypress(g_hx_editor);
			}
		}
	}
	return 0;
}

/* ── Embedded editor (no menu bar) ─────────────────────────────────
 * Called by EEPROM GUI (or other fullscreen apps) to run the hex
 * editor inside an existing alt-screen.  The caller owns rows
 * 1..ext_header_rows (menu bar, config bar, separator).  hx draws
 * its column headers + hex content from ext_header_rows+1 onwards.
 *
 * Returns: HX_EMBED_QUIT  — user quit (:q, Ctrl-Q)
 *          HX_EMBED_F10   — user pressed F10 (wants parent menu)
 * ----------------------------------------------------------------- */
int hx_run_embedded(const char *filename, int ext_header_rows) {
	hx_vt100_keys_init();
	g_hx_editor = editor_init();
	/* ext_header_rows = rows owned by caller;
	 * hx draws 2 more (column headers + separator) */
	g_hx_editor->header_rows = ext_header_rows + 2;
	editor_openfile(g_hx_editor, filename);

	/* Skip-redraw snapshot (same optimisation as hx_run) */
	int prev_cx = 0, prev_cy = 0, prev_line = 0;
	unsigned int prev_len = 0;
	bool prev_dirty = false;
	enum editor_mode prev_mode = MODE_NORMAL;
	int result = HX_EMBED_QUIT;

	while (1) {
		tx_fifo_wait_drain();
		editor_refresh_screen(g_hx_editor);
		hx_io_write(1, "\x1b[?25l", 6);

		prev_cx    = g_hx_editor->cursor_x;
		prev_cy    = g_hx_editor->cursor_y;
		prev_line  = g_hx_editor->line;
		prev_len   = g_hx_editor->content_length;
		prev_dirty = g_hx_editor->dirty;
		prev_mode  = g_hx_editor->mode;

		do {
			editor_process_keypress(g_hx_editor);
			if (g_hx_editor->quit_requested) break;
			if (g_hx_editor->menu_pending) {
				g_hx_editor->menu_pending = false;
				result = HX_EMBED_F10;
				goto done;
			}
		} while (spsc_queue_level(&rx_fifo) > 0);
		if (g_hx_editor->quit_requested) break;

		/* Skip-redraw loop */
		while (g_hx_editor->cursor_x      == prev_cx   &&
		       g_hx_editor->cursor_y      == prev_cy   &&
		       g_hx_editor->line          == prev_line  &&
		       g_hx_editor->content_length == prev_len  &&
		       g_hx_editor->dirty         == prev_dirty &&
		       g_hx_editor->mode          == prev_mode) {
			editor_process_keypress(g_hx_editor);
			if (g_hx_editor->quit_requested) break;
			if (g_hx_editor->menu_pending) {
				g_hx_editor->menu_pending = false;
				result = HX_EMBED_F10;
				goto done;
			}
			while (spsc_queue_level(&rx_fifo) > 0) {
				editor_process_keypress(g_hx_editor);
			}
		}
	}
done:
	result = g_hx_editor->quit_requested ? HX_EMBED_QUIT : result;
	return result;
}

/* ── Granular embedded editor API ───────────────────────────────
 * Unlike hx_run_embedded(), these give the caller full control over
 * the render/keypress loop so the hex editor can coexist with other
 * UI elements (config bar, menus, etc.).
 * ----------------------------------------------------------------- */

struct editor* hx_embed_init(int header_rows) {
	hx_vt100_keys_init();
	g_hx_editor = editor_init();
	g_hx_editor->header_rows = header_rows;
	/* Allocate a 1-byte empty buffer so rendering never hits NULL */
	g_hx_editor->contents = malloc(1);
	g_hx_editor->contents[0] = '\0';
	g_hx_editor->content_length = 0;
	return g_hx_editor;
}

void hx_embed_load(const char* filename) {
	if (!g_hx_editor) return;
	/* Free previous contents */
	if (g_hx_editor->filename) { free(g_hx_editor->filename); g_hx_editor->filename = NULL; }
	if (g_hx_editor->contents) { free(g_hx_editor->contents); g_hx_editor->contents = NULL; }
	if (g_hx_editor->paged_path) { free(g_hx_editor->paged_path); g_hx_editor->paged_path = NULL; }
	g_hx_editor->content_length = 0;
	g_hx_editor->line = 0;
	g_hx_editor->cursor_x = 1;
	g_hx_editor->cursor_y = 1;
	g_hx_editor->dirty = false;
	g_hx_editor->quit_requested = false;
	g_hx_editor->paged = false;
	g_hx_editor->file_size = 0;
	g_hx_editor->page_offset = 0;
	editor_setmode(g_hx_editor, MODE_NORMAL);
	editor_openfile(g_hx_editor, filename);
}

void hx_embed_load_buffer(void *buf, unsigned int len, const char *label) {
	if (!g_hx_editor) return;
	/* Free previous contents */
	if (g_hx_editor->filename) { free(g_hx_editor->filename); g_hx_editor->filename = NULL; }
	if (g_hx_editor->contents) { free(g_hx_editor->contents); g_hx_editor->contents = NULL; }
	if (g_hx_editor->paged_path) { free(g_hx_editor->paged_path); g_hx_editor->paged_path = NULL; }
	g_hx_editor->content_length = 0;
	g_hx_editor->line = 0;
	g_hx_editor->cursor_x = 1;
	g_hx_editor->cursor_y = 1;
	g_hx_editor->dirty = false;
	g_hx_editor->quit_requested = false;
	g_hx_editor->paged = false;
	g_hx_editor->file_size = 0;
	g_hx_editor->page_offset = 0;
	editor_setmode(g_hx_editor, MODE_NORMAL);

	/* Take ownership of the caller-provided arena buffer */
	g_hx_editor->contents = (char*)buf;
	g_hx_editor->content_length = len;

	/* Set display label as filename */
	if (label) {
		g_hx_editor->filename = malloc(strlen(label) + 1);
		if (g_hx_editor->filename)
			strncpy(g_hx_editor->filename, label, strlen(label) + 1);
	}

	editor_statusmessage(g_hx_editor, STATUS_INFO,
		"\"%s\" (%u bytes)", label ? label : "buffer", len);
}

struct editor* hx_embed_editor(void) {
	return g_hx_editor;
}

void hx_cleanup(void) {
	if (g_hx_editor) {
		editor_free(g_hx_editor);
		g_hx_editor = NULL;
	}
}

#endif /* BUSPIRATE */
