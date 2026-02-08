/**
 * @file flash.h
 * @brief SPI flash memory command interface.
 * @details Provides commands for SPI flash programming and reading.
 */

/**
 * @brief Flash/program SPI flash memory.
 * @param res  Command result structure
 */
void flash(struct command_result* res);

/**
 * @brief Load data from SPI flash to file.
 * @param res  Command result structure
 */
void load(struct command_result* res);