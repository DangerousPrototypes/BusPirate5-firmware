/**
 * @file ddr5.h
 * @brief DDR5 SPD (Serial Presence Detect) decoder command interface.
 * @details Provides command to read and decode DDR5 memory module SPD data.
 */

/**
 * @brief Handler for DDR5 SPD decoder.
 * @param res  Command result structure
 */
void ddr5_handler(struct command_result* res);