/**
 * @file binio_helpers.h
 * @brief Binary mode I/O helper functions.
 * @details Provides utility functions for binary protocol script handling.
 */

/**
 * @brief Reset script state.
 */
void script_reset(void);

/**
 * @brief Send script data.
 * @param c    Data buffer
 * @param len  Data length
 */
void script_send(const char* c, uint32_t len);
