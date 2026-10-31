/**
 * @file h_help.h
 * @brief Help command interface (h command).
 * @details Provides help display for global and mode-specific commands.
 */

/**
 * @brief Display mode-specific help.
 */
void help_mode(void);

/**
 * @brief Display global commands help.
 */
void help_global(void);

extern const struct bp_command_def help_def;

/**
 * @brief Handler for help command (h).
 * @param res  Command result structure
 */
void help_handler(struct command_result* res);
