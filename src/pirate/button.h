/**
 * @file button.h
 * @brief Hardware button (EXT1) interface with press detection.
 * @details Provides GPIO-based button handling with IRQ-based press detection.
 *          Distinguishes between short and long presses.
 *          Platform differences:
 *          - RP2040: Active high with pulldown (normal GPIO)
 *          - RP2350: Active low with pullup (E9 silicon bug workaround)
 */

#pragma once

/**
 * @brief Button press type codes.
 */
enum button_codes {
    BP_BUTT_NO_PRESS = 0,  ///< No press detected
    BP_BUTT_SHORT_PRESS,   ///< Short press (<550ms)
    BP_BUTT_LONG_PRESS,    ///< Long press (≥550ms)
    BP_BUTT_MAX            ///< Total count of press types
};

/**
 * @brief Initialize button GPIO and pull resistor.
 * @note Configures EXT1 pin with platform-appropriate pull configuration.
 */
void button_init(void);

/**
 * @brief Enable button interrupt with custom callback.
 * @param button_id  Button identifier (currently unused, always EXT1)
 * @param callback   IRQ callback function
 * @note Enables both edge interrupts (rise and fall).
 */
void button_irq_enable(uint8_t button_id, gpio_irq_callback_t callback);

/**
 * @brief Disable button interrupt.
 * @param button_id  Button identifier (currently unused, always EXT1)
 */
void button_irq_disable(uint8_t button_id);

/**
 * @brief Example IRQ callback handler for button events.
 * @param gpio    GPIO pin number
 * @param events  GPIO event mask (rise/fall)
 * @note This is a reference implementation; copy for custom use.
 */
void button_irq_callback(uint gpio, uint32_t events);

/**
 * @brief Poll button GPIO state directly.
 * @param button_id  Button identifier (currently unused, always EXT1)
 * @return true if button is pressed, false otherwise
 */
bool button_get(uint8_t button_id);

/**
 * @brief Check for button press and retrieve press type.
 * @param button_id  Button identifier (currently unused, always EXT1)
 * @return Button press code (BP_BUTT_NO_PRESS, SHORT, or LONG)
 * @note Clears the press flag after reading.
 */
enum button_codes button_check_press(uint8_t button_id);
