/**
 * @file pause.h
 * @brief Pause/delay command interface.
 * @details Provides command to pause execution for specified duration.
 */

/**
 * @brief Handler for pause command.
 * @param res  Command result structure
 * @note Syntax: pause <ms> - Pause for specified milliseconds
 */
void pause_handler(struct command_result* res);