/**
 * @file dummy.h
 * @brief Dummy/test command interface.
 * @details Provides placeholder command for testing.
 */

extern const struct bp_command_def dummy_def;

/**
 * @brief Handler for dummy test command.
 * @param res  Command result structure
 */
void dummy_handler(struct command_result* res);