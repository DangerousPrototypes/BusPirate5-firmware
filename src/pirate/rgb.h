/**
 * @file rgb.h
 * @brief RGB LED (WS2812) control and animation effects.
 * @details Controls 16-18 WS2812 RGB LEDs arranged around the PCB perimeter using PIO.
 *          Supports various visual effects based on PCB geometry and polar coordinates.
 */

#pragma once

/**
 * @brief Available RGB LED animation effects.
 * @details Effects utilize the physical position and angle of each LED on the PCB.
 *          LED_EFFECT_DISABLED must be 0 for boolean compatibility.
 *          LED_EFFECT_PARTY_MODE must be the last effect for cycling.
 */
typedef enum _led_effect {
    LED_EFFECT_DISABLED = 0,       ///< LEDs off
    LED_EFFECT_SOLID = 1,          ///< Solid color, no animation
    LED_EFFECT_ANGLE_WIPE = 2,     ///< Wipe based on angular position
    LED_EFFECT_CENTER_WIPE = 3,    ///< Wipe from center outward
    LED_EFFECT_CLOCKWISE_WIPE = 4, ///< Clockwise rotational wipe
    LED_EFFECT_TOP_SIDE_WIPE = 5,  ///< Wipe from top/side edges
    LED_EFFECT_SCANNER = 6,        ///< Cylon/KITT scanner effect
    LED_EFFECT_GENTLE_GLOW = 7,    ///< Smooth breathing effect

    LED_EFFECT_PARTY_MODE,         ///< Cycles through all effects (must be last)
    MAX_LED_EFFECT,                ///< Total count of effects
} led_effect_t;
#define DEFAULT_LED_EFFECT = LED_EFFECT_GENTLE_GLOW;

static_assert(LED_EFFECT_DISABLED == 0,
              "LED_EFFECT_DISABLED must be zero"); // when used as boolean, also relied upon in party mode handling
static_assert(MAX_LED_EFFECT - 1 == LED_EFFECT_PARTY_MODE, "LED_EFFECT_PARTY_MODE must be the last effect");

/**
 * @brief Initialize RGB LED hardware and PIO state machine.
 * @note Must be called before any other RGB operations.
 */
void rgb_init(void);

/**
 * @brief Send a single 24-bit RGB color word to the LED chain.
 * @param color  GRB-packed 24-bit color value (WS2812 format)
 * @warning Low-level function, prefer rgb_set_all() or rgb_set_array().
 */
void rgb_put(uint32_t color);

/**
 * @brief Enable or disable RGB animation timer interrupts.
 * @param enable  true to enable animation updates, false to disable
 */
void rgb_irq_enable(bool enable);

/**
 * @brief Set all LEDs to the same RGB color.
 * @param r  Red intensity (0-255)
 * @param g  Green intensity (0-255)
 * @param b  Blue intensity (0-255)
 */
void rgb_set_all(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Set the active LED animation effect.
 * @param new_effect  Effect to activate from led_effect_t enum
 */
void rgb_set_effect(led_effect_t new_effect);

/**
 * @brief Set LEDs from an array of RGB values.
 * @param colors  Array of GRB-packed 24-bit color values
 * @note Array must contain at least RGB_LEN elements.
 */
void rgb_set_array(uint32_t* colors);
