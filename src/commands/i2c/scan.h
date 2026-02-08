/**
 * @file scan.h
 * @brief I2C address scan command interface.
 * @details Provides command to scan for I2C devices on the bus.
 */

/**
 * @brief Scan I2C bus for devices.
 * @param res  Command result structure
 */
void i2c_search_addr(struct command_result* res);