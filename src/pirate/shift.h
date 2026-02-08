/**
 * @file shift.h
 * @brief 74HC595 shift register control interface.
 * @details Controls two cascaded 74HC595 shift registers via SPI for I/O expansion.
 *          Provides 16 output pins for controlling hardware like backlight, reset,
 *          PSU enable, current control, and analog multiplexer selection.
 *          Used only on BP5 Rev8/Rev9; Rev10+ uses direct GPIO.
 */

/**
 * @brief Initialize shift register control pins (enable, latch).
 * @note Does not initialize SPI - that must be done separately.
 */
void shift_init(void);

/**
 * @brief Enable or disable shift register outputs.
 * @param enable  true to enable outputs, false to tri-state all outputs
 */
void shift_output_enable(bool enable);

/**
 * @brief Modify shift register bits with clear/set operations.
 * @param clear_bits  Bitmask of bits to clear (set to 0)
 * @param set_bits    Bitmask of bits to set (set to 1)
 * @param busy_wait   true to use SPI spinlock, false to proceed asynchronously
 * @note Clear operation happens before set operation.
 */
void shift_clear_set(uint16_t clear_bits, uint16_t set_bits, bool busy_wait);

/**
 * @brief Modify shift register bits with SPI spinlock enabled.
 * @param clear_bits  Bitmask of bits to clear
 * @param set_bits    Bitmask of bits to set
 * @note Convenience wrapper for shift_clear_set() with busy_wait=true.
 */
void shift_clear_set_wait(uint16_t clear_bits, uint16_t set_bits);
