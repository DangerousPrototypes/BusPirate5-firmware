/**
 * @file irtxrx.h
 * @brief Infrared transmit/receive command interface.
 * @details Provides commands for IR remote control transmission and reception.
 */

/**
 * @brief Infrared receive command handler.
 * @param res  Command result structure
 */
void irrx_handler(struct command_result *res);

/**
 * @brief Infrared transmit command handler.
 * @param res  Command result structure
 */
void irtx_handler(struct command_result *res);

extern const struct bp_command_def irtx_def;
extern const struct bp_command_def irrx_def;