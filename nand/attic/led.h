/**
 * @file		led.h
 * @author		Andrew Loebs
 * @brief		Header file of the LED module
 *
 * Handles interaction with the "LD3" LED on the NUCLEO-L432KC board.
 *
 */

#ifndef __LED_H
#define __LED_H

#include <stdbool.h>

#include "../st/ll/stm32l4xx_ll_gpio.h"

#define LD3_PORT GPIOB
#define LD3_PIN  LL_GPIO_PIN_3

/// @brief Enables GPIOB clock and sets up LED pin
void led_init(void);

/// @brief Sets the LED output
/// @note This function is inlined as it will be called from exception handlers
static inline void led_set_output(bool pin_state)
{
    if (pin_state) {
        LL_GPIO_SetOutputPin(LD3_PORT, LD3_PIN);
    }
    else {
        LL_GPIO_ResetOutputPin(LD3_PORT, LD3_PIN);
    }
}

#endif // __LED_H
