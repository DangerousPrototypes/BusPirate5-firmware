/**
 * @file jep106_lookup.h
 * @brief JEP106 manufacturer code lookup command interface.
 * @details Provides command to decode JEP106 manufacturer IDs.
 */

/**
 * @brief Handler for JEP106 lookup command.
 * @param res  Command result structure
 */
void jep106_handler(struct command_result* res);extern const struct bp_command_def jep106_def;