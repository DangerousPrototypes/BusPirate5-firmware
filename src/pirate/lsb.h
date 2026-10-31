/**
 * @file lsb.h
 * @brief LSB/MSB bit order conversion utilities.
 * @details Provides functions to convert between LSB and MSB bit ordering.
 */

/**
 * @brief Convert number to LSB format.
 * @param d         Data value
 * @param num_bits  Number of bits
 * @return          Converted value
 */
uint32_t lsb_convert(uint32_t d, uint8_t num_bits);

/**
 * @brief Get number in MSB or LSB format.
 * @param d          Data value pointer (modified in place)
 * @param num_bits   Number of bits
 * @param bit_order  Bit order (0=MSB, 1=LSB)
 */
void lsb_get(uint32_t* d, uint8_t num_bits, bool bit_order);