/**
 * @file eeprom_i2c.h
 * @brief I2C EEPROM command interface.
 * @details Provides command for I2C EEPROM programming and reading.
 */

/**
 * @brief I2C EEPROM handler.
 * @param res  Command result structure
 */
void i2c_eeprom_handler(struct command_result* res);

extern const struct bp_command_def eeprom_i2c_def;

