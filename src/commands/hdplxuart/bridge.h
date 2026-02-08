/**
 * @file bridge.h
 * @brief Half-duplex UART bridge command interface.
 * @details Provides transparent bridge mode for half-duplex UART communication.
 */

/**
 * @brief Half-duplex UART bridge handler.
 * @param res  Command result structure
 */
void hduart_bridge_handler(struct command_result* res);