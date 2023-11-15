/**
 * @file		uart.h
 * @author		Andrew Loebs
 * @brief		Header file of the UART module
 *
 * Handles interaction with the UART module. Functions should not be called directly --
 * this module provides functionality for c std lib functions (putc, getc, printf, etc.).
 *
 * This is not intended to be a high-speed UART driver, it does have blocking functions.
 *
 */

#ifndef __UART_H
#define __UART_H

#include <stdbool.h>

/// @brief Enables GPIOA and USART clocks, sets up pins and USART2 module.
/// @note Called in main() -- cannot be called by __libc_init_array() since
///  system clock is not configured
void uart_init(void);

/// @brief Writes a character to the UART
/// @note Called by _write sys call
void _uart_putc(char c);

/// @brief Returns true and writes character to c if present, false if not
/// @note Called by _read sys call
bool _uart_try_getc(char *c);

/// @brief Handles uart interrupt
/// @note Called by USART2_IRQHandler
void _uart_isr(void);

#endif // __UART_H
