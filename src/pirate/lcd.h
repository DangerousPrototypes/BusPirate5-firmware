/**
 * @file lcd.h
 * @brief LCD display hardware control interface.
 * @details Low-level hardware control for LCD (chip select, backlight, reset).
 *          Does not include display driver communication (see display/ folder).
 */

/**
 * @brief Initialize LCD hardware pins (CS, D/P, backlight, reset).
 * @note Must be called before LCD communication.
 */
void lcd_init(void);

/**
 * @brief Enable or disable LCD backlight.
 * @param enable  true to turn backlight on, false to turn off
 */
void lcd_backlight_enable(bool enable);

/**
 * @brief Perform hardware reset of LCD per datasheet timing.
 * @note Pulls RESET low for 20μs, then waits 100ms for startup.
 */
void lcd_reset(void);