/**
 * @file logic.h
 * @brief Logic analyzer command interface.
 * @details Provides command to capture and display logic analyzer data.
 */

/**
 * @brief Handler for logic analyzer command.
 * @param res  Command result structure
 * @note Captures digital signals on I/O pins and exports to sigrok format
 */
void logic_handler(struct command_result* res);
extern const struct bp_command_def logic_def;