/**
 * @file i2c.h
 * @brief I2C mode-specific commands interface.
 * @details Provides I2C mode commands for device interaction.
 */

/**
 * @brief Handler for I2C dump command.
 * @param res  Command result structure
 */
void i2c_dump_handler(struct command_result* res);
extern const struct bp_command_def i2c_dump_def;