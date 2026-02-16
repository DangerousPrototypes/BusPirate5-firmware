/**
 * @file bluetag.h
 * @brief BlueTag JTAG/SWD pinout discovery command interface.
 * @details Provides automated detection and identification of JTAG and SWD pinouts.
 */

/**
 * @brief BlueTag JTAG/SWD pinout finder handler.
 * @param res  Command result structure
 */
void bluetag_handler(struct command_result* res);

extern const struct bp_command_def bluetag_def;