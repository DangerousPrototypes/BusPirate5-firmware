/**
 * @file macro.h
 * @brief Macro script execution command interface.
 * @details Provides command to load and execute macro scripts from files.
 */

/**
 * @brief Handler for macro command.
 * @param res  Command result structure
 * @note Syntax: macro \<#\> [-f \<file\>] [-l] [-h]
 */
void macro_handler(struct command_result* res);