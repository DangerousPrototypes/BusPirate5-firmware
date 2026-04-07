/**
 * @file comment.h
 * @brief Comment line command interface.
 * @details Lines starting with # are silently ignored, allowing comment lines
 *          to be pasted from a text editor without causing errors.
 */

extern const struct bp_command_def comment_def;

/**
 * @brief Handler for the # comment command (no-op).
 * @param res  Command result structure
 */
void comment_handler(struct command_result* res);
