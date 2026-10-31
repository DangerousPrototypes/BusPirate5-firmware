/**
 * @file system_monitor.h
 * @brief System voltage and current monitoring interface.
 * @details Provides continuous monitoring of voltage and current measurements.
 */

/**
 * @brief Execute system monitor update.
 * @return  true if update performed
 */
bool monitor(void);

/**
 * @brief Initialize system monitor.
 */
void monitor_init(void);

/**
 * @brief Reset monitor state.
 */
void monitor_reset(void);

/**
 * @brief Force immediate monitor update.
 */
void monitor_force_update(void);

/**
 * @brief Get voltage character for specific pin and digit.
 * @param pin    Pin number
 * @param digit  Digit position
 * @param c      Output character
 * @return       true on success
 */
bool monitor_get_voltage_char(uint8_t pin, uint8_t digit, char* c);

/**
 * @brief Get voltage string pointer for pin.
 * @param pin  Pin number
 * @param c    Output string pointer
 * @return     true on success
 */
bool monitor_get_voltage_ptr(uint8_t pin, char** c);

/**
 * @brief Get current string pointer.
 * @param c  Output string pointer
 * @return   true on success
 */
bool monitor_get_current_ptr(char** c);

/**
 * @brief Get current character for digit.
 * @param digit  Digit position
 * @param c      Output character
 * @return       true on success
 */
bool monitor_get_current_char(uint8_t digit, char* c);

/**
 * @brief Clear voltage display buffer.
 */
void monitor_clear_voltage(void);

/**
 * @brief Clear current display buffer.
 */
void monitor_clear_current(void);

/**
 * @brief Check if voltage reading changed.
 * @return  true if changed
 */
bool monitor_voltage_changed(void);

/**
 * @brief Check if current reading changed.
 * @return  true if changed
 */
bool monitor_current_changed(void);
