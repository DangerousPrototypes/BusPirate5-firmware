/**
 * @file ui_process.h
 * @brief Command processing interface.
 * @details Provides main command and syntax processing functions.
 */

/**
 * @brief Process command-mode commands.
 * @return true on success
 */
bool ui_process_commands(void);

/**
 * @brief Process syntax-mode scripts.
 * @return true on success
 */
bool ui_process_syntax(void);