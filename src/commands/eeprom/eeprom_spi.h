/**
 * @file eeprom_spi.h
 * @brief SPI EEPROM command interface.
 * @details Provides command for SPI EEPROM programming and reading.
 */

/**
 * @brief SPI EEPROM handler.
 * @param res  Command result structure
 */
void spi_eeprom_handler(struct command_result* res);
extern const struct bp_command_def eeprom_spi_def;

