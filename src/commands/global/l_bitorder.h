/**
 * @file l_bitorder.h
 * @brief Bit order control commands (l/L commands).
 * @details Provides commands to set MSB-first or LSB-first bit order
 *          for data transfer and display.
 */

/**
 * @brief Set bit order to MSB-first.
 */
void bitorder_msb(void);

/**
 * @brief Handler for MSB-first command (L).
 * @param res  Command result structure
 * @note Displays: MSB 0b10000000
 */
void bitorder_msb_handler(struct command_result* res);

/**
 * @brief Set bit order to LSB-first.
 */
void bitorder_lsb(void);

/**
 * @brief Handler for LSB-first command (l).
 * @param res  Command result structure
 * @note Displays: LSB 0b00000001
 */
void bitorder_lsb_handler(struct command_result* res);