/**
 * @file ovrclk.h
 * @brief System clock overclock command interface.
 * @details Provides command to adjust system clock frequency.
 */

extern const struct bp_command_def ovrclk_def;

/**
 * @brief Handler for overclock command.
 * @param res  Command result structure
 */
void ovrclk_handler(struct command_result* res);