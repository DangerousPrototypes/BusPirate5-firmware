/**
 * @file bridge.h
 * @brief UART transparent bridge command interface.
 * @details Provides command to bridge UART to USB for passthrough communication.
 */

/**
 * @brief UART transparent bridge mode.
 * @param res  Command result structure
 */
void uart_bridge_handler(struct command_result* res);

extern const struct bp_command_def uart_bridge_def;