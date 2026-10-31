/**
 * @file hash.h
 * @brief Hash/comment command interface.
 * @details '#' silently ignores the rest of the line, allowing users to
 *          paste annotated command sequences from a text editor or webpage.
 */

extern const struct bp_command_def hash_def;

void hash_handler(struct command_result* res);
