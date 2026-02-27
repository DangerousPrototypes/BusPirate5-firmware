/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */

#ifndef HX_EDITOR_H
#define HX_EDITOR_H

#include "charbuf.h"

#include <stdbool.h>

/*
 * Mode the editor can be in.
 */
enum editor_mode {
	MODE_APPEND        = 1 << 0, // append value after the current cursor position.
	MODE_APPEND_ASCII  = 1 << 1, // append literal typed value after the current cursor position.
	MODE_REPLACE_ASCII = 1 << 2, // replace literal typed value over the current cursor position.
	MODE_NORMAL        = 1 << 3, // normal mode i.e. for navigating, commands.
	MODE_INSERT        = 1 << 4, // insert values at cursor position.
	MODE_INSERT_ASCII  = 1 << 5, // insert literal typed value at cursor position.
	MODE_REPLACE       = 1 << 6, // replace values at cursor position.
	MODE_COMMAND       = 1 << 7, // command input mode.
	MODE_SEARCH        = 1 << 8, // search mode.
};

/*
 * Search directions.
 */
enum search_direction {
	SEARCH_FORWARD,
	SEARCH_BACKWARD,
};

/*
 * Current status severity.
 */
enum status_severity {
	STATUS_INFO,    // appear as lightgray bg, black fg
	STATUS_WARNING, // appear as yellow bg, black fg
	STATUS_ERROR,   // appear as red bg, white fg
};

#define INPUT_BUF_SIZE 80

/*
 * This struct contains internal information of the state of the editor.
 */
struct editor {
	int octets_per_line; // Amount of octets (bytes) per line. Ideally multiple of 2.
	int grouping;        // Amount of bytes per group. Ideally multiple of 2.

	int line;        // The 'line' in the editor. Used for scrolling.
	int cursor_x;    // Cursor x pos on the current screen
	int cursor_y;    // Cursor y pos on the current screen
	int screen_rows; // amount of screen rows after init
	int screen_cols; // amount of screen columns after init

	enum editor_mode mode; // mode the editor is in

	bool         dirty;          // whether the buffer is modified
	char*        filename;       // the filename currently open
	char*        contents;       // the file's contents
	unsigned int content_length; // length of the contents

	enum status_severity status_severity;     // status severity
	char                 status_message[120]; // status message

	char inputbuffer[INPUT_BUF_SIZE]; // input buffer for commands
	                                  // or search strings etc.
	int inputbuffer_index; // the index of the current typed key shiz.

	char searchstr[INPUT_BUF_SIZE]; // the current search string or NULL if none.

	struct action_list* undo_list; // tail of the list

#ifdef BUSPIRATE
	bool menu_pending;  // F10 was pressed, main loop should open menu
#endif
};

/*
 * Initializes the editor struct with basic values.
 */
struct editor* editor_init();

void editor_cursor_at_offset(struct editor* e, int offset, int* x, int *y);
void editor_delete_char_at_cursor(struct editor* e);
void editor_delete_char_at_offset(struct editor* e, unsigned int offset);
void editor_free(struct editor* e);
void editor_increment_byte(struct editor* e, int amount);
void editor_insert_byte(struct editor* e, char x, bool after);
void editor_insert_byte_at_offset(struct editor* e, unsigned int offset, char x, bool after);
void editor_move_cursor(struct editor* e, int dir, int amount);
int editor_offset_at_cursor(struct editor* e);
void editor_openfile(struct editor* e, const char* filename);
void editor_process_command(struct editor* e, const char* cmd);
void editor_process_search(struct editor* e, const char* str, enum search_direction dir);
int editor_parse_search_string(const char* inputstr, struct charbuf* parsedstr,
			       const char** err_info);
void editor_process_keypress(struct editor* e);
int editor_read_hex_input(struct editor* e, char* output);
int editor_read_string(struct editor* e, char* dst, int len);
void editor_render_ascii(struct editor* e, int rownum, unsigned int start_offset, struct charbuf* b);
void editor_render_contents(struct editor* e, struct charbuf* b);
void editor_render_help(struct editor* e);
void editor_render_ruler(struct editor* e, struct charbuf* buf);
void editor_render_status(struct editor* e, struct charbuf* buf);
void editor_refresh_screen(struct editor* e);
void editor_replace_byte(struct editor* e, char x);
void editor_replace_byte_at_offset(struct editor* e, unsigned int offset, char x);
void editor_scroll(struct editor* e, int units);
void editor_scroll_to_offset(struct editor* e, unsigned int offset);
void editor_setmode(struct editor *e, enum editor_mode mode);
int editor_statusmessage(struct editor* e, enum status_severity s, const char* fmt, ...);
void editor_undo(struct editor* e);
void editor_redo(struct editor* e);
void editor_writefile(struct editor* e);

#endif // _HX_EDITOR_H
