/**
 * @file		shell.h
 * @author		Andrew Loebs
 * @brief		Header file of the shell module
 *
 * Provides functionality for a virtual shell & CLI.
 *
 */

#ifndef __SHELL_H
#define __SHELL_H

#include <stdlib.h> // size_t

/// @brief Initializes the shell
void shell_init(void);

/// @brief Gives processing time to the shell
void shell_tick(void);

/// @brief Writes characters to the shell
void shell_print(const char *buff, size_t len);

/// @brief Writes a string to the shell
void shell_prints(const char *string);

/// @brief Writes a string + newline to the shell
void shell_prints_line(const char *string);

/// @brief Writes formatted output to the shell
void shell_printf(const char *format, ...);

/// @brief Writes formatted output + newline to the shell
void shell_printf_line(const char *format, ...);

/// @brief Writes a newline character to the shell
/// @note This keeps our newline in a single function in case we decide to use a different line
/// ending in the future
void shell_put_newline(void);

#endif // __SHELL_H
