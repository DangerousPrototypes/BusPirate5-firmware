#pragma once

typedef enum _led_effect {
    LED_EFFECT_DISABLED = 0,
    LED_EFFECT_SOLID = 1,
    LED_EFFECT_ANGLE_WIPE = 2,
    LED_EFFECT_CENTER_WIPE = 3,
    LED_EFFECT_CLOCKWISE_WIPE = 4,
    LED_EFFECT_TOP_SIDE_WIPE = 5,
    LED_EFFECT_SCANNER = 6,
    LED_EFFECT_GENTLE_GLOW = 7,

    LED_EFFECT_PARTY_MODE, // NOTE: This must be the last effect
    MAX_LED_EFFECT,
} led_effect_t;
#define DEFAULT_LED_EFFECT = LED_EFFECT_GENTLE_GLOW;

static_assert(LED_EFFECT_DISABLED == 0, "LED_EFFECT_DISABLED must be zero"); // when used as boolean, also relied upon in party mode handling
static_assert(MAX_LED_EFFECT-1 == LED_EFFECT_PARTY_MODE, "LED_EFFECT_PARTY_MODE must be the last effect");

// TODO: review and provide a more useful client-focused API.
// TODO: adjust to use RGB color type instead of uint32_t.

// TODO: some method to "claim" subset of the pixels for a particular effect
//       Examples:
//       * top or bottom row (e.g., for a progress bar)
//       * all pixels (e.g., for clock or infinite progress bar)
void rgb_init(void);
void rgb_put(uint32_t color);
void rgb_irq_enable(bool enable);
void rgb_set_all(uint8_t r, uint8_t g, uint8_t b);
void rgb_set_masked(uint8_t r, uint8_t g, uint8_t b, uint32_t pixel_mask);
void rgb_set_pixels(uint32_t* colors, uint32_t count);
void rgb_set_effect(led_effect_t new_effect);
