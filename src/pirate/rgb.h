#pragma once

#define BP_HW_RGB_HAS_ALL_PIXELS !(BP_VER==5 && BP_REV<=9)

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

static_assert(LED_EFFECT_DISABLED == 0,
              "LED_EFFECT_DISABLED must be zero"); // when used as boolean, also relied upon in party mode handling
static_assert(MAX_LED_EFFECT - 1 == LED_EFFECT_PARTY_MODE, "LED_EFFECT_PARTY_MODE must be the last effect");


#if 1 
//BP_HW_RGB_HAS_ALL_PIXELS
// REV 10+
enum {
    LED_BOTTOM_CENTER_UP = 0,
    LED_BOTTOM_RIGHT_SIDE,
    LED_BOTTOM_RIGHT_UP,
    LED_RIGHT_BOTTOM_UP,
    LED_RIGHT_BOTTOM_SIDE,
    LED_RIGHT_TOP_SIDE,
    LED_RIGHT_TOP_UP,
    LED_TOP_RIGHT_UP,
    LED_TOP_RIGHT_SIDE,
    LED_TOP_CENTER_UP,
    LED_TOP_LEFT_SIDE,
    LED_TOP_LEFT_UP,
    LED_LEFT_TOP_UP,
    LED_LEFT_TOP_SIDE,
    LED_LEFT_BOTTOM_SIDE,
    LED_LEFT_BOTTOM_UP,
    LED_BOTTOM_LEFT_UP,
    LED_BOTTOM_LEFT_SIDE,
};
#else
// REV 8
enum {
    LED_BOTTOM_RIGHT_SIDE = 0,
    LED_BOTTOM_RIGHT_UP,
    LED_RIGHT_BOTTOM_UP,
    LED_RIGHT_BOTTOM_SIDE,
    LED_RIGHT_TOP_SIDE,
    LED_RIGHT_TOP_UP,
    LED_TOP_RIGHT_UP,
    LED_TOP_RIGHT_SIDE,
    LED_TOP_CENTER_UP,
    LED_TOP_LEFT_SIDE,
    LED_TOP_LEFT_UP,
    LED_LEFT_TOP_UP,
    LED_LEFT_TOP_SIDE,
    LED_LEFT_BOTTOM_SIDE,
    LED_LEFT_BOTTOM_UP,
    LED_BOTTOM_LEFT_UP,
};

#define LED_BOTTOM_CENTER_UP 0x21 //push up 33 bits, so no impact
#define LED_BOTTOM_LEFT_SIDE 0x21 //push up 33 bits, so no impact

#endif

struct _led_overlay {
    uint8_t gpio;
    uint32_t pixels;
    uint8_t r_on;
    uint8_t g_on;
    uint8_t b_on;
    uint8_t r_off;
    uint8_t g_off;
    uint8_t b_off;
    uint32_t irq_type;
    uint32_t on_delay;
    uint32_t off_delay;
};

// TODO: review and provide a more useful client-focused API.
// TODO: adjust to use RGB color type instead of uint32_t.
void rgb_init(void);
void rgb_put(uint32_t color);
void rgb_irq_enable(bool enable);
void rgb_set_all(uint8_t r, uint8_t g, uint8_t b);
void rgb_set_effect(led_effect_t new_effect);
void assign_pixel_overlay_function(const struct _led_overlay *overlay);
void remove_pixel_overlay_function(const struct _led_overlay *overlay);
