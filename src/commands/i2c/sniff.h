/**
 * @file sniff.h
 * @brief I2C bus sniffer command interface.
 * @details Provides command to monitor I2C bus traffic.
 */

/**
 * @brief Sniff I2C bus traffic.
 * @param res  Command result structure
 */
void i2c_sniff(struct command_result* res);