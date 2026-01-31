/**
 * @file script.h
 * @brief Script execution command interface.
 * @details Provides commands to execute script files with pause/comment options.
 */

/**
 * @brief Execute script file.
 * @param location         Script file path
 * @param pause_for_input  Pause for user input between commands
 * @param show_comments    Display comment lines
 * @param show_tip         Show tip message
 * @param exit_on_error    Exit script on first error
 * @return true on success
 */
bool script_exec(char* location, bool pause_for_input, bool show_comments, bool show_tip, bool exit_on_error);

/**
 * @brief Handler for script command.
 * @param res  Command result structure
 */
void script_handler(struct command_result* res);