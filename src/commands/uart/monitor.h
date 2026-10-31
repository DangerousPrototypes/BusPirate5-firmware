/**
 * @file monitor.h
 * @brief UART monitor command interface.
 * @details Provides command to monitor and display UART traffic.
 */

/**
 * @brief UART traffic monitor.
 * @param res  Command result structure
 */
void uart_monitor_handler(struct command_result* res);

extern const struct bp_command_def uart_monitor_def;