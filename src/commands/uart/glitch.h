/**
 * @file glitch.h
 * @brief UART glitching attack command interface.
 * @details Provides command to perform timing glitch attacks on UART.
 */

#ifndef UART_GLITCH

/**
 * @brief UART glitching attack mode.
 * @param res  Command result structure
 */
void uart_glitch_handler(struct command_result* res);

#define UART_GLITCH
#endif // UART_GLITCH