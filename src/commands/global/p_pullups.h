/**
 * @file p_pullups.h
 * @brief Programmable pullup/pulldown resistor command interface (p/P commands).
 * @details Provides commands to enable/disable pullup or pulldown resistors
 *          on I/O pins with selectable values (1.3K to 1M ohms).
 */

/**
 * @brief Initialize pullup resistor subsystem.
 */
void pullups_init(void);

/**
 * @brief Handler for pullup enable command (P).
 * @param res  Command result structure
 * @note Syntax: P [value] [-d] [-p <pins>]
 */
void pullups_enable_handler(struct command_result* res);

/**
 * @brief Handler for pullup disable command (p).
 * @param res  Command result structure
 */
void pullups_disable_handler(struct command_result* res);

/**
 * @brief Enable pullup resistors with default configuration.
 */
void pullups_enable(void);

/**
 * @brief Disable all pullup/pulldown resistors.
 */
void pullups_disable(void);