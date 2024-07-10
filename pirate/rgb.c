#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"
#include "system_config.h"
#include "pirate/rgb.h"

#define RGB_MAX_BRIGHT 32

// Externally to this file, colors are always handled as uint32_t values
// which stores the color in 0x00RRGGBB format.
// Internally to this file, the color is stored in the CPIXEL_COLOR_xxx format.
// This reduces type-confusion that results in incorrect colors.

typedef struct _CPIXEL_COLOR_RGB {
    // pad to 32-bits b/c pio_sm_put_blocking() expects a uint32_t
    // This also ensures the compiler can optimize away conversion
    // from constexpr uint32_t colors (0x00RRGGBB format) into this
    // type into a noop (same underlying structure)
    uint8_t _unused;
    union {
        uint8_t red;
        uint8_t r;
    };
    union {
        uint8_t green;
        uint8_t g;
    };
    union {
        uint8_t blue;
        uint8_t b;
    };
} CPIXEL_COLOR_RGB;

typedef struct _CPIXEL_COLOR_GRB {
    uint8_t _unused;
    union {
        uint8_t green;
        uint8_t g;
    };
    union {
        uint8_t red;
        uint8_t r;
    };
    union {
        uint8_t blue;
        uint8_t b;
    };
} CPIXEL_COLOR_GRB;

// C23 would allow this to be `constexpr`
// here, `static const` relies on compiler to optimize this away
static const CPIXEL_COLOR_RGB RGBCOLOR_BLACK = { .r=0x00, .g=0x00, .b=0x00 };

static CPIXEL_COLOR_RGB color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    CPIXEL_COLOR_RGB c = { .r = r, .g = g, .b = b };
    return c;
}
static CPIXEL_COLOR_GRB color_grb(uint8_t g, uint8_t r, uint8_t b) {
    CPIXEL_COLOR_GRB c = { .g = g, .r = r, .b = b };
    return c;
}
static CPIXEL_COLOR_RGB rgb_from_rgb(CPIXEL_COLOR_RGB c) { return c; }
static CPIXEL_COLOR_GRB grb_from_grb(CPIXEL_COLOR_GRB c) { return c; }
static CPIXEL_COLOR_RGB rgb_from_grb(CPIXEL_COLOR_GRB c) {
    CPIXEL_COLOR_RGB r = { .r = c.r, .g = c.g, .b = c.b };
    return r;
}
static CPIXEL_COLOR_GRB grb_from_rgb(CPIXEL_COLOR_RGB c) {
    CPIXEL_COLOR_GRB r = { .g = c.g, .r = c.r, .b = c.b };
    return r;
}
static CPIXEL_COLOR_RGB rgb_from_uint32(uint32_t c) {
    CPIXEL_COLOR_RGB r = {
        ._unused = (c >> 24) & 0xff, // carry the input data, to allow compiler to optimization this to a noop
        .r =       (c >> 16) & 0xff,
        .g =       (c >>  8) & 0xff,
        .b =       (c >>  0) & 0xff,
    };
    return r;
}
static CPIXEL_COLOR_GRB grb_from_uint32(uint32_t c) {
    CPIXEL_COLOR_GRB r = { .g = (c >> 8) & 0xff, .r = (c >> 16) & 0xff, .b = c & 0xff };
    return r;
}
static uint32_t rgb_as_uint32(CPIXEL_COLOR_RGB c) {
    return (c.r << 16) | (c.g << 8) | c.b;
}
static uint32_t grb_as_uint32(CPIXEL_COLOR_GRB c) {
    return (c.r << 16) | (c.g << 8) | c.b;
}

// Use C11 `_Generic` to auto-select the correct conversion function
// based on the color type ... simplifies writing correct code.
#define rgb_from_color(COLOR)                       \
    _Generic((COLOR),                               \
        CPIXEL_COLOR_RGB: rgb_from_rgb,             \
        CPIXEL_COLOR_GRB: rgb_from_grb,             \
    )(COLOR)
#define grb_from_color(COLOR)                       \
    _Generic((COLOR),                               \
        CPIXEL_COLOR_RGB: grb_from_rgb,             \
        CPIXEL_COLOR_GRB: grb_from_grb,             \
    )(COLOR)
#define uint32_from_color(COLOR)                    \
    _Generic((COLOR),                               \
        CPIXEL_COLOR_RGB: rgb_as_uint32,            \
        CPIXEL_COLOR_GRB: grb_as_uint32,            \
    )(COLOR)





// Note that both the layout and overall count of pixels
// has changed between revisions.  As a result, the count
// of elements for any of these arrays may differ.
//
// groups_top_left[]:
//    defines led_bitmasks that generally start at the top left corner of the device,
//    and continue in a diagnol pattern.  Generally setup in pairs, although some groups
//    may set three pixels at a time.
//
// groups_center_left[]:
//    tbd
//
// groups_center_clockwise[]:
//    tbd
//
// groups_top_down[]:
//    tbd
//
// Also add a new constant, COUNT_OF_PIXELS, to define the number
// pixels on each revision of board.

// Total count of RGB pixels
#define COUNT_OF_PIXELS RGB_LEN
#if BP5_REV <= 9
    // Sadly, C still doesn't recognize the following format as constexpr enough for static_assert()
    //static const uint8_t COUNT_OF_PIXELS = 16u;
    //static const uint32_t PIXEL_MASK_UPPER = 0b011001101011001101;
    //static const uint32_t PIXEL_MASK_SIDE  = 0b100110010100110010;

    static_assert(COUNT_OF_PIXELS < sizeof(uint32_t)*8, "Too many pixels for pixel mask definition to be valid")
    // Pixels that shine in direction  of OLED: idx 0,    3,4,    7,  9,      12,13,
    static const uint32_t PIXEL_MASK_UPPER = 0b0011 0010 1001 1001;
    // Pixels that shine    orthogonal to OLED: idx   1,2,    5,6,  8,  10,11,      14,15,
    static const uint32_t PIXEL_MASK_SIDE  = 0b1100 1101 0110 0110;

    static const uint32_t groups_top_left[] = {
        // clang-format off
        ((1u <<  1) | (1u <<  2)             ),
        ((1u <<  0) | (1u <<  3)             ),
        ((1u <<  4) | (1u <<  5) | (1u << 15)),
        ((1u <<  6) | (1u <<  7) | (1u << 14)),
        ((1u <<  8) | (1u << 13)             ),
        ((1u <<  9) | (1u << 12)             ),
        ((1u << 10) | (1u << 11)             ),
        // clang-format on
    };
    static const uint32_t groups_center_left[] = {
        // clang-format off
        ((1u <<  3) | (1u <<  4)                         ),
        ((1u <<  2) | (1u <<  5)                         ),
        ((1u <<  1) | (1u <<  6)                         ),
        ((1u <<  0) | (1u <<  7) | (1u <<  8) | (1 <<  9)),
        ((1u << 10) | (1u << 15)                         ),
        ((1u << 11) | (1u << 14)                         ),
        ((1u << 12) | (1u << 13)                         ),
        // clang-format on
    };  
    static const uint32_t groups_center_clockwise[] = {
        // clang-format off
        ((1u << 13) | (1u << 14)),
        ((1u << 15)             ),
        ((1u <<  0) | (1u <<  1)),
        ((1u <<  2) | (1u <<  3)),
        ((1u <<  4) | (1u <<  5)),
        ((1u <<  6) | (1u <<  7)),
        ((1u <<  8) | (1u <<  9)),
        ((1u << 10)             ),
        ((1u << 11) | (1u << 12)),
        // clang-format on
    };
#elif BP5_REV >= 10
    // Sadly, C still doesn't recognize the following format as constexpr enough for static_assert()
    // and thus we must resort to preprocessor macros (still).
    //static const uint32_t PIXEL_MASK_UPPER = 0b011001101011001101;
    //static const uint32_t PIXEL_MASK_SIDE  = 0b100110010100110010;

    // Pixels that shine in direction  of OLED: idx 0,  2,3,    6,7,  9,   11,12,      15,16
    #define PIXEL_MASK_UPPER (0b011001101011001101)
    // Pixels that shine    orthogonal to OLED: idx   1,    4,5,    8,  10,      13,14,     17
    #define PIXEL_MASK_SIDE  (0b100110010100110010)


    static const uint32_t groups_top_left[] = {
        // clang-format off
        (               (1u <<  2) | (1u <<  3)),
        (               (1u <<  1) | (1u <<  4)),
        ((1u << 17)   | (1u <<  0) | (1u <<  5)),
        ((1u << 16)   |              (1u <<  6)),
        ((1u << 15)   |              (1u <<  7)),
        ((1u << 14)   | (1u <<  9) | (1u <<  8)),
        ((1u << 13)   | (1u << 10) | (1u <<  5)), // BUGBUG -- LED 5 is likely a typo?
        ((1u << 12)   | (1u << 11)             ),
        // clang-format on
    };
    static const uint32_t groups_center_left[] = {
        // clang-format off
        ((1u <<  4) | (1u <<  5)),
        ((1u <<  3) | (1u <<  6)),
        ((1u <<  2) | (1u <<  7)),
        ((1u <<  1) | (1u <<  8)),
        ((1u <<  0) | (1u <<  9)),
        ((1u << 17) | (1u << 10)),
        ((1u << 16) | (1u << 11)),
        ((1u << 15) | (1u << 12)),
        ((1u << 14) | (1u << 13)),
        // clang-format on
    };  
    static const uint32_t groups_center_clockwise[] = {
        // clang-format off
        ((1u << 14) | (1u << 15)),
        ((1u << 16)             ),
        ((1u << 17) | (1u <<  0)),
        ((1u <<  1) | (1u <<  2)),
        ((1u <<  3) | (1u <<  4)),
        ((1u <<  5) | (1u <<  6)),
        ((1u <<  7)             ),
        ((1u <<  8) | (1u <<  9)),
        ((1u << 10) | (1u << 11)),
        ((1u << 12) | (1u << 13)),
        // clang-format on
    };
#endif

static const uint32_t groups_top_down[] = {
    PIXEL_MASK_UPPER,
    PIXEL_MASK_SIDE,
}; // MSb is last led in string...

#define PIXEL_MASK_ALL ((1u << COUNT_OF_PIXELS) - 1)

static_assert(COUNT_OF_PIXELS < sizeof(uint32_t)*8, "Too many pixels for pixel mask definition to be valid");
static_assert((PIXEL_MASK_UPPER & PIXEL_MASK_SIDE) == 0, "Pixel cannot be both upper and side");
static_assert((PIXEL_MASK_UPPER | PIXEL_MASK_SIDE) == PIXEL_MASK_ALL, "Pixel must be either upper or side");

uint32_t leds[COUNT_OF_PIXELS];

static inline void rgb_send(void){  
    // TODO: define symbolic constant for which state machine (no magic numbers!)
    for(int i=0; i<RGB_LEN; i++) {
        // little-endian, so 0x00GGRRBB  is stored as 0xBB 0xRR 0xGG 0x00
        // Shifting it left by 8 bits will give bytes 0x00 0xBB 0xRR 0xGG
        // which allows the PIO to unshift the bytes in the correct order
        pio_sm_put_blocking(pio1, 3, ((leds[i]) << 8u));
    }
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r) << 8) |
         ((uint32_t)(g) << 16) |
         (uint32_t)(b);
}


/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t color_wheel(uint8_t pos) {
  pos = 255 - pos;
  if(pos < 85) {
    return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (pos * 3);
  } else if(pos < 170) {
    pos -= 85;
    return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
  } else {
    pos -= 170;
    return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (0);
  }   
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 * The ..._div() version of the function divides each of
 * the R/G/B values by the system config's led brightness.
 */
uint32_t color_wheel_div(uint8_t pos) {
    pos = 255 - pos;
    uint8_t r,g,b;
    if(pos < 85) {
        r=((uint32_t)(255 - pos * 3));
        g=((uint32_t)(0) );
        b=(pos * 3);
    } else if(pos < 170) {
        pos -= 85;
        r=((uint32_t)(0) );
        g=((uint32_t)(pos * 3) );
        b=(255 - pos * 3);
    } else {
        pos -= 170;
        r=((uint32_t)(pos * 3));
        g=((uint32_t)(255 - pos * 3) );
        b=(0);
    }

    return
        // clang-format off
        ( (g/system_config.led_brightness_divisor) << 16) |
        ( (r/system_config.led_brightness_divisor) <<  8) |
        ( (b/system_config.led_brightness_divisor)      ) ;
        // clang-format on
}

// BUGBUG -- Many of the callers convert an RGB color to GRB before passing
//           it to this function.  This means the color parameter is GRB (not RGB)?
// TODO: define RGB and GRB structures, and use them for clarity rather than uint32_t
void rgb_assign_grb_color(uint32_t index_mask, uint32_t grb_color){
    for (int i = 0; i < RGB_LEN; i++){
        if (index_mask & (1u << i)) {
            leds[i] = grb_color;
        }
    }
}

//something like this to cycle, delay, return done
//This needs some more documentation:
//  groups            the pixel groups; a pointer to an array of index_masks,
//                    each index_mask indicating which LEDs are considered
//                    part of the group
//  group_count       count of elements in the `groups` array
//  color_wheel       a function pointer; Function must take a single byte
//                    parameter and return a ***GRB-formatted*** color
//                    -- BUGBUG -- Verify color order is as expected?
//  color_count       the distinct seed values for color_wheel() parameter.
//                    rgb_master() will ensure the parameter is always in
//                    range [0..color_count-1]
//  color_increment   the PER GROUP increment of colors for the color_wheel()
//                    parameter
//This function returns:
//  false             if additional iterations are needed for the animation
//  true              sufficient iterations have been completed
//
// HACKHACK -- to ensure a known starting state (reset the static variables)
//             can call with group_count=0, cycles=0
// TODO: Rename `color_wheel` to more appropriate, generic name
bool rgb_master(
    const uint32_t *groups,
    uint8_t group_count,
    uint32_t (*color_wheel)(uint8_t color),
    uint8_t color_count,
    uint8_t color_increment,
    uint8_t cycles,
    uint8_t delay_ms
    )
{
    static uint8_t color_idx = 0;
    static uint16_t completed_cycles = 0;

    for (int i = 0; i < group_count; i++) {
        // group_count     is uint8_t (1..255)
        // color_increment is uint8_t (0..255)
        // color_count     is uint8_t (1..255)
        // therefore, maximum value of tmp_color is
        // color_count + (group_count * color_increment)
        // === 255 + (255 * 255) = 65010
        // This value could fit in uint16_t ... but just use PICO-native 32-bits
        uint32_t tmp_color_idx = color_idx + (i*color_increment);
        tmp_color_idx %= color_count; // ensures safe to cast to uint8_t

        uint32_t rgb_color = color_wheel((uint8_t)tmp_color_idx);
        rgb_assign_grb_color(groups[i], rgb_color);
    }
    rgb_send();
    ++color_idx;

    //finished one complete cycle
    if (color_idx == color_count) {
        color_idx = 0;
        ++completed_cycles;
        if (completed_cycles >= cycles) {
            completed_cycles = 0;
            return true;
        }        
    }

    return false;
}

struct repeating_timer rgb_timer;

bool rgb_scanner(void) {
    // TODO: decode this animation and add notes on what it's intended result is
    static_assert(count_of(groups_center_left) < (sizeof(uint16_t)*8), "uint16_t too small to hold count_of(groups_center_left) elements");

    static uint16_t pixel_bitmask = 1u << (count_of(groups_center_left) - 1);
    static uint8_t frame_delay_count = 0u;
    static uint8_t color_idx = 0;
    
    const uint32_t colors[] = {
        0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
        0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
        0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
        0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B,
    };

    if(frame_delay_count){
        --frame_delay_count;
        return false;
    }

    uint32_t color_grb = 0;
    // clang-format off
    color_grb |= ( (((colors[color_idx] & 0xff0000) / system_config.led_brightness_divisor) & 0xff0000) >> 8); // swap R/G
    color_grb |= ( (((colors[color_idx] & 0x00ff00) / system_config.led_brightness_divisor) & 0x00ff00) << 8); // swap R/G
    color_grb |= ( (((colors[color_idx] & 0x0000ff) / system_config.led_brightness_divisor) & 0x0000ff)     ); // B remains in place
    // clang-format on
    for (int i = 0; i < count_of(groups_center_left); i++) {
        // only use the above color for those pixels in the bitmask
        uint32_t tmp_color = (pixel_bitmask & (1u << i)) ? color_grb : 0x0a0a0a;
        rgb_assign_grb_color(groups_center_left[i], tmp_color);
    }
    rgb_send();
    
    if (pixel_bitmask == 0) {
        // no more bits set, so done with a cycle
        // set a longer delay before starting the next cycle
        frame_delay_count = 0xF0;
        // reset the pixel bitmask 
        pixel_bitmask = (0x01 << (count_of(groups_center_left)));
        color_idx++;
        // if gone through all the colors,
        // then re-initialize static variables (reset state)
        if (color_idx == count_of(colors)) {
            color_idx = 0;
            pixel_bitmask = pixel_bitmask >> 1;
            return true;
        }
    } else {
        // stay on each frame for eight calls
        frame_delay_count = 0x8;
    }
    pixel_bitmask = pixel_bitmask >> 1;
    return false;
}

bool rgb_timer_callback(struct repeating_timer *t){
    static uint8_t mode=2;

    uint32_t color_grb;
    bool next=false;

    if (system_config.led_effect < MAX_LED_EFFECT) {
        mode = system_config.led_effect;
    }
    
    // clang-format off
    switch(mode) {
        case LED_EFFECT_DISABLED:
            rgb_assign_grb_color(0xffffffff, 0x000000);
            rgb_send();  
            break;          
        case LED_EFFECT_SOLID:
            // NOTE: swaps from RGB (typical stored value) to GRB
            //       because that is the order the Pixels expect
            // clang-format off
            color_grb  = ( (((system_config.led_color & 0xff0000) / system_config.led_brightness_divisor) & 0xff0000) >> 8); // swap R/G
            color_grb |= ( (((system_config.led_color & 0x00ff00) / system_config.led_brightness_divisor) & 0x00ff00) << 8); // swap R/G
            color_grb |= ( (((system_config.led_color & 0x0000ff) / system_config.led_brightness_divisor) & 0x0000ff)     ); // B remains in place
            // clang-format on
            rgb_assign_grb_color(0xffffffff, color_grb);
            rgb_send();
            break;
        case LED_EFFECT_ANGLE_WIPE:
            next = rgb_master(groups_top_left,         count_of(groups_top_left),         &color_wheel_div, 0xff, (0xff/count_of(groups_top_left)        ), 5, 10);
            break;
        case LED_EFFECT_CENTER_WIPE:
            next = rgb_master(groups_center_left,      count_of(groups_center_left),      &color_wheel_div, 0xff, (0xff/count_of(groups_center_left)     ), 5, 10);
            break;
        case LED_EFFECT_CLOCKWISE_WIPE:
            next = rgb_master(groups_center_clockwise, count_of(groups_center_clockwise), &color_wheel_div, 0xff, (0xff/count_of(groups_center_clockwise)), 5, 10);
            break;
        case LED_EFFECT_TOP_SIDE_WIPE:
            next = rgb_master(groups_top_down,         count_of(groups_top_down),         &color_wheel_div, 0xff, (                                    30), 5, 10);
            break;
        case LED_EFFECT_SCANNER:
            next = rgb_scanner();
            break;
        case LED_EFFECT_PARTY_MODE:
            assert(!"Party mode should never be value of the local mode variable!");
            break;
    }
    // clang-format on

    if (system_config.led_effect == LED_EFFECT_PARTY_MODE && next) {
        static_assert(LED_EFFECT_DISABLED == 0, "LED_EFFECT_DISABLED must be zero");
        static_assert(MAX_LED_EFFECT-1 == LED_EFFECT_PARTY_MODE, "LED_EFFECT_PARTY_MODE must be the last effect");
        ++mode;
        if (mode >= LED_EFFECT_PARTY_MODE) {
            mode = LED_EFFECT_DISABLED+1;
        }
    }

    return true;
}


void rgb_irq_enable(bool enable){
    static bool enabled=false;
    if(enable && !enabled)
    {
        add_repeating_timer_ms(-10, rgb_timer_callback, NULL, &rgb_timer);
        enabled=true;
    }
    else if(!enable && enabled)
    {
        cancel_repeating_timer(&rgb_timer);
        enabled = false;
    }
}

void rgb_init(void)
{
    // RGB LEDs driven by PIO0
    gpio_set_function(RGB_CDO, GPIO_FUNC_PIO1);  

    PIO pio = pio1;
    int sm = 3;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, RGB_CDO, 800000, false);

    for (int i=0;i<RGB_LEN; i++){
        leds[i]=0x00;
    }

    // Create a repeating timer that calls repeating_timer_callback.
    // If the delay is negative (see below) then the next call to the callback will be exactly 500ms after the
    // start of the call to the last callback
    // Negative delay so means we will call repeating_timer_callback, and call it again
    // 500ms later regardless of how long the callback took to execute
    rgb_irq_enable(true);
};


void rgb_set_all(uint8_t r, uint8_t g, uint8_t b){
    uint32_t color =
    // clang-format off
        ( (g / system_config.led_brightness_divisor) << 16) |
        ( (r / system_config.led_brightness_divisor) <<  8) |
        ( (b / system_config.led_brightness_divisor) <<  0) ;
    // clang-format on
    rgb_assign_grb_color(0xffffffff, color);
    rgb_send();
}

//function to control LED from led mode onboard demo
#define DEMO_LED 1
void rgb_put(uint32_t color)
{

    for (int i=0;i<RGB_LEN; i++){
        leds[i]=0;
    }
    leds[DEMO_LED]=color & 0xffffff; //urgb_u32(color>>16,(color>>8)&0xff,color&0xff);
    rgb_send();

};