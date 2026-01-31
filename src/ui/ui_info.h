/**
 * @file ui_info.h
 * @brief Hardware information display interface.
 * @details Provides functions to display pin configuration, names,
 *          labels, and voltage measurements.
 */

/**
 * @brief Print toolbar with current mode and settings.
 */
void ui_info_print_toolbar(void);

/**
 * @brief Print I/O pin names (IO0-IO7).
 */
void ui_info_print_pin_names(void);

/**
 * @brief Print I/O pin labels (MOSI, MISO, CLK, etc.).
 */
void ui_info_print_pin_labels(void);

/**
 * @brief Print I/O pin voltage measurements.
 * @param refresh  Force ADC refresh if true
 */
void ui_info_print_pin_voltage(bool refresh);
