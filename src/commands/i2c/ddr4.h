/**
 * @file ddr4.h
 * @brief DDR4 SPD (Serial Presence Detect) decoder command interface.
 * @details Provides command to read and decode DDR4 memory module SPD data.
 */

/**
 * @brief Handler for DDR4 SPD decoder.
 * @param res  Command result structure
 */
void ddr4_handler(struct command_result* res);
extern const struct bp_command_def ddr4_def;