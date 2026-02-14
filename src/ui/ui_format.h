/**
 * @file ui_format.h
 * @brief Number formatting and bit ordering utilities interface.
 * @details Provides functions for formatting numbers in various bases,
 *          reordering bits (MSB/LSB), and display formatting.
 */

struct command_attributes;  // Forward declaration

/**
 * @brief Reorder bits according to current bit order setting.
 * @param d  Value to reorder
 * @return Reordered value
 */
uint32_t ui_format_bitorder(uint32_t d);

/**
 * @brief Print number with command attributes.
 * @param attributes  Command attributes structure
 * @param value       Pointer to value to print
 */
void ui_format_print_number_2(struct command_attributes* attributes, uint32_t* value);

/**
 * @brief Print number with specified bit width and format.
 * @param value          Value to print
 * @param num_bits       Number of bits
 * @param display_format Display format (hex/dec/bin)
 */
void ui_format_print_number_3(uint32_t value, uint32_t num_bits, uint32_t display_format);

/**
 * @brief Manually reorder bits with specified order.
 * @param d         Pointer to value
 * @param num_bits  Number of bits
 * @param bit_order Bit order (true=MSB, false=LSB)
 */
void ui_format_bitorder_manual(uint32_t* d, uint8_t num_bits, bool bit_order);

/**
 * @brief Convert value to LSB-first order.
 * @param d         Value to convert
 * @param num_bits  Number of bits
 * @return LSB-first value
 */
uint32_t ui_format_lsb(uint32_t d, uint8_t num_bits);