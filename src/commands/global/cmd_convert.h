/**
 * @file cmd_convert.h
 * @brief Number base conversion command interface.
 * @details Provides commands to convert numbers between hex/dec/bin formats.
 */

/**
 * @brief Convert and display value in all bases.
 * @param value     Value to convert
 * @param num_bits  Number of bits
 */
void cmd_convert_base(uint32_t value, uint32_t num_bits);

/**
 * @brief Handler for base conversion command (=).
 * @param res  Command result structure
 */
void cmd_convert_base_handler(struct command_result* res);

/**
 * @brief Handler for bitwise inverse command (~).
 * @param res  Command result structure
 */
void cmd_convert_inverse_handler(struct command_result* res);
