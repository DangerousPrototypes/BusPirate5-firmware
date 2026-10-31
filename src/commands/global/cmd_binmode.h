/**
 * @file cmd_binmode.h
 * @brief Binary mode command interface.
 * @details Provides command to enter binary protocol mode for scripting.
 */

/**
 * @brief Handler for binary mode command.
 * @param res  Command result structure
 * @note Enters binary protocol mode for automated control
 */
void cmd_binmode_handler(struct command_result* res);
extern const struct bp_command_def cmd_binmode_def;
