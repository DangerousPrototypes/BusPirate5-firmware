/**
 * @file bio.c
 * @brief Buffered I/O (BIO) pin control implementation
 * 
 * This file implements the buffered I/O system for Bus Pirate, providing
 * bidirectional level-shifted access to the 8 main I/O pins. Each BIO pin
 * has a buffer IC with direction control, allowing safe interfacing with
 * devices at different voltage levels.
 * 
 * The BIO system provides:
 * - Direction control (input/output)
 * - GPIO override control
 * - Drive strength configuration
 * - Protected debug UART pin handling
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"

/**
 * @brief Initialize all buffered I/O pins
 * 
 * Configures all BIO pins and their associated buffer direction pins.
 * Sets up:
 * - GPIO override to normal
 * - Drive strength to 2mA
 * - Initial direction to input
 * - Buffer direction to input
 * 
 * @note Skips pins configured for debug UART to avoid conflicts
 * @note Must be called during system initialization
 */
void bio_init(void) {
    // setup the buffer IO and DIRection pins
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        /*#ifdef BP_DEBUG_ENABLED
        if(i==BP_DEBUG_UART_TX || i==BP_DEBUG_UART_RX) continue;
        #endif*/
        if (system_config.pin_func[i + 1] == BP_PIN_DEBUG) {
            continue;
        }

        gpio_set_inover(1u << bio2bufiopin[i], GPIO_OVERRIDE_NORMAL);
        gpio_set_outover(1u << bio2bufiopin[i], GPIO_OVERRIDE_NORMAL);
        gpio_set_drive_strength(bio2bufiopin[i], GPIO_DRIVE_STRENGTH_2MA);
        gpio_set_dir(bio2bufiopin[i], GPIO_IN);
        gpio_set_function(bio2bufiopin[i], GPIO_FUNC_SIO);
    }

    // init buffer direction (0 = input)
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        // don't blow up our debug UART settings
        /*#ifdef BP_DEBUG_ENABLED
        if(i==BP_DEBUG_UART_TX || i==BP_DEBUG_UART_RX) continue;
        #endif*/
        if (system_config.pin_func[i + 1] == BP_PIN_DEBUG) {
            continue;
        }
        gpio_set_inover(1u << bio2bufdirpin[i], GPIO_OVERRIDE_NORMAL);
        gpio_set_outover(1u << bio2bufdirpin[i], GPIO_OVERRIDE_NORMAL);
        gpio_put(bio2bufdirpin[i], BUFDIR_INPUT);
        gpio_set_dir(bio2bufdirpin[i], GPIO_OUT);
        gpio_set_function(bio2bufdirpin[i], GPIO_FUNC_SIO);
    }
}

/**
 * @brief Initialize a single buffer direction pin
 * 
 * @param bio BIO pin number (0-7)
 */
void bio_buf_pin_init(uint8_t bio) {
    gpio_put(bio2bufdirpin[bio], BUFDIR_INPUT);
    gpio_set_dir(bio2bufdirpin[bio], GPIO_OUT);
    gpio_set_function(bio2bufdirpin[bio], GPIO_FUNC_SIO);
}

/**
 * @brief Set buffer direction to output
 * 
 * Configures the buffer IC to allow data flow from RP2040 to external device.
 * 
 * @param bio BIO pin number (0-7)
 * @note Does not change the RP2040 GPIO direction
 */
void bio_buf_output(uint8_t bio) {
    gpio_put(bio2bufdirpin[bio], BUFDIR_OUTPUT); // make the buffer an output
}

/**
 * @brief Set buffer direction to input
 * 
 * Configures the buffer IC to allow data flow from external device to RP2040.
 * 
 * @param bio BIO pin number (0-7)
 * @note Does not change the RP2040 GPIO direction
 */
void bio_buf_input(uint8_t bio) {
    gpio_put(bio2bufdirpin[bio], BUFDIR_INPUT); // make the buffer an input
}

/**
 * @brief Configure BIO pin and buffer for output
 * 
 * Sets both the buffer direction and GPIO direction to output in the correct order.
 * 
 * @param bio BIO pin number (0-7)
 * @note Buffer is set to output first, then GPIO to prevent glitches
 */
void bio_output(uint8_t bio) {
    // first set the buffer to output
    gpio_put(bio2bufdirpin[bio], BUFDIR_OUTPUT);
    // now set pin to output
    gpio_set_dir(bio2bufiopin[bio], GPIO_OUT);
}

/**
 * @brief Configure BIO pin and buffer for input
 * 
 * Sets both the GPIO direction and buffer direction to input in the correct order.
 * 
 * @param bio BIO pin number (0-7)
 * @note GPIO is set to input first, then buffer to prevent current flow
 */
void bio_input(uint8_t bio) {
    // first set the pin to input
    gpio_set_dir(bio2bufiopin[bio], GPIO_IN);
    // now set buffer to input
    gpio_put(bio2bufdirpin[bio], BUFDIR_INPUT);
}

/**
 * @brief Set output value of BIO pin
 * 
 * @param bio BIO pin number (0-7)
 * @param value true for high, false for low
 * 
 * @note Pin must be configured as output first
 * @todo Track buffer state and manipulate as needed
 */
void bio_put(uint8_t bio, bool value) {
    gpio_put(bio2bufiopin[bio], value);
}
/**
 * @brief Read current value of BIO pin
 * 
 * @param bio BIO pin number (0-7)
 * @return true if pin is high, false if low
 */
bool bio_get(uint8_t bio) {
    return gpio_get(bio2bufiopin[bio]);
}

/**
 * @brief Set GPIO function for BIO pin
 * 
 * Allows assigning alternate functions (SPI, I2C, UART, PIO, etc) to BIO pins.
 * 
 * @param bio BIO pin number (0-7)
 * @param function GPIO function code (GPIO_FUNC_SPI, GPIO_FUNC_PIO0, etc)
 */
void bio_set_function(uint8_t bio, uint8_t function) {
    gpio_set_function(bio2bufiopin[bio], function);
}
