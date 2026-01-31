/**
 * @file i_info.h
 * @brief System information display command (i command).
 * @details Displays Bus Pirate hardware/firmware info, storage, pin voltages.
 */

/**
 * @brief Handler for info command (i).
 * @param res  Command result structure
 * @note Displays: hardware version, firmware version, MCU ID, storage, pin voltages
 */
void i_info_handler(struct command_result* res);