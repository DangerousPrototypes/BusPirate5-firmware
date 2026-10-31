/*
 * This file is part of hx - a hex editor for the terminal.
 *
 * Copyright (c) 2016 Kevin Pors. See LICENSE for details.
 */

#ifndef HX_UNDO_H
#define HX_UNDO_H

/*
 * Contains definitions and functions to allow undo/redo actions.
 * It's basically a double-linked list, where the tail is the last
 * action done, and the head the first. curr points to the last
 * performed action - so undoing undoes curr and redoing redoes
 * curr->next.
 */

// Type of undo/redo actions.
enum action_type {
	ACTION_DELETE,  // character deleted
	ACTION_INSERT,  // character inserted
	ACTION_REPLACE, // character replaced
	ACTION_APPEND   // character appended
};

/* The status of the position that curr is currently at. */
enum curr_pos {
	BEFORE_HEAD,  // curr is NULL and before head.
	NODE,         // curr points to an action.
	AFTER_TAIL,   // curr is NULL and after tail.
	NOTHING       // curr is NULL and none of the above.
};

/*
 * This struct contains the data about an action, as well as the pointers
 * to the previous action (or NULL if this is the first), the next action
 * (or NULL if this is the first or last), the type of action, the offset
 * where the action was done, and the character which was deleted, inserted
 * or replaced.
 */
struct action {
	struct action* prev; // previous action or NULL if first.
	struct action* next; // next action or NULL if first, or last.

	enum action_type act; // the type of action.
	int offset;           // the offset where something was changed.
	unsigned char c;      // the character inserted, deleted, etc.
};


/*
 * Simply returns the name of the action corresponding to the given enum type.
 */
const char* action_type_name(enum action_type type);

/*
 * The action_list contains head and tail information.
 */
struct action_list {
	struct action* head;  // Head/start of the list.
	struct action* curr;  // Current position within the list.
	enum curr_pos curr_status; // Meta position of curr.
	struct action* tail;  // Tail/end of the list.
};

struct action_list* action_list_init();
void action_list_add(struct action_list* list, enum action_type type, int offset, unsigned char c);
void action_list_delete(struct action_list* list, struct action* action);
void action_list_free(struct action_list* list);
void action_list_print(struct action_list* list);
unsigned int action_list_size(struct action_list* list);
unsigned int action_list_curr_pos(struct action_list* list);
void action_list_move(struct action_list* list, int direction);

#endif // HX_UNDO_H
