/**
 * @file smps.h
 * @brief SMPS (switching power supply) control command interface.
 * @details Provides command to control internal SMPS for efficiency.
 */

/**
 * @brief Handler for SMPS control command.
 * @param res  Command result structure
 * @note Controls internal buck-boost converter on/off
 */
void smps_handler(struct command_result* res);
extern const struct bp_command_def smps_def;