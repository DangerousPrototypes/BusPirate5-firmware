/**
 * @file comment.h
 * @brief Comment line command interface.
 * @details Lines beginning with '#' are treated as comments and ignored.
 */

extern const struct bp_command_def comment_def;

/**
 * @brief Handler for '#' comment lines — does nothing.
 * @param res  Command result structure
 */
void comment_handler(struct command_result* res);
