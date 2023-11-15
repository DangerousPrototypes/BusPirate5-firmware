/**
 * @file		led.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the LED module
 *
 */

#include "led.h"

#include "../st/ll/stm32l4xx_ll_bus.h"

// public function definitions
void led_init(void)
{
    // enable peripheral clock
    if (!LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOB))
        LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

    // setup pin as output -- default to low (LED off)
    led_set_output(false);
    LL_GPIO_SetPinMode(LD3_PORT, LD3_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(LD3_PORT, LD3_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(LD3_PORT, LD3_PIN, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(LD3_PORT, LD3_PIN, LL_GPIO_PULL_NO);
}
