#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"
#include "system_config.h"

#define RGB_MAX_BRIGHT 32

// COLOR_RGB COLOR_GRB

// Externally to this file, colors are always handled as uint32_t values
// which stores the color in 0x00RRGGBB format.
// Internally to this file, the color is stored in the CPIXEL_COLOR_xxx format.
// This reduces type-confusion that results in incorrect colors.

typedef struct _CPIXEL_COLOR_RGB {
    union {
        uint8_t r;
        uint8_t red;
    };
    union {
        uint8_t g;
        uint8_t green;
    };
    union {
        uint8_t b;
        uint8_t blue;
    };
} CPIXEL_COLOR_RGB;

// C23 would allow this to be `constexpr`
// here, `static const` relies on compiler to optimize this away (and remove entirely if unused)
static const CPIXEL_COLOR_RGB RGBCOLOR_BLACK = { .r=0x00, .g=0x00, .b=0x00 };

static inline CPIXEL_COLOR_RGB color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    CPIXEL_COLOR_RGB c = { .r = r, .g = g, .b = b };
    return c;
}
static inline CPIXEL_COLOR_RGB rgb_from_rgb(CPIXEL_COLOR_RGB c) { return c; }
static inline CPIXEL_COLOR_RGB rgb_from_uint32(uint32_t c) {
    CPIXEL_COLOR_RGB r = {
        .r =       (c >> 16) & 0xff,
        .g =       (c >>  8) & 0xff,
        .b =       (c >>  0) & 0xff,
    };
    return r;
}
static inline uint32_t         rgb_as_uint32(CPIXEL_COLOR_RGB c) {
    return (c.r << 16) | (c.g << 8) | c.b;
}

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
#if BP5_REV <= 9
    static const uint8_t COUNT_OF_PIXELS = 16;
    const uint32_t groups_top_left[]={
        ((1u <<  1)              | (1u <<  2)),
        ((1u <<  0)              | (1u <<  3)),
        ((1u <<  4) | (1u <<  5) | (1u << 15)),
        ((1u <<  6) | (1u <<  7) | (1u << 14)),
        ((1u <<  8)              | (1u << 13)),
        ((1u <<  9)              | (1u << 12)),
        ((1u << 10)              | (1u << 11)),
    };
    const uint32_t groups_center_left[]={
        ((1u <<  3) | (1u <<  4)),
        ((1u <<  2) | (1u <<  5)),
        ((1u <<  1) | (1u <<  6)),
        ((1u <<  0) | (1u <<  7) | (1u <<  8) | (1 <<  9)), // BUGBUG -- this seems ... off?  combines 7/8/9 as one logical pixel for this?
        ((1u << 15) | (1u << 10)),
        ((1u << 14) | (1u << 11)),
        ((1u << 13) | (1u << 12)),
    };
    const uint32_t groups_center_clockwise[]={
        ((1u << 13) | (1u << 14)),
        ((1u << 15)             ), 
        ((1u <<  0) | (1u <<  1)),
        ((1u <<  2) | (1u <<  3)),
        ((1u <<  4) | (1u <<  5)),
        ((1u <<  6) | (1u <<  7)),
        ((1u <<  8) | (1u <<  9)),
        ((1u << 10)             ), 
        ((1u << 11) | (1u << 12)),
    };
    const uint32_t groups_top_down[]={ //MSB is last led in string...
        0b0011001010011001,
        0b1100110101100110,
        //|...|...|...|... == 16 bits == COUNT_OF_PIXELS
    }; 
#elif BP5_REV >= 10

    static const uint8_t COUNT_OF_PIXELS = 18;
    const uint32_t groups_top_left[]={
        ((1u <<  2) | (1u <<  3)),
        ((1u <<  1) | (1u <<  4)),
        ((1u <<  0) | (1u <<  5) | (1u << 17)), // three LEDs at a time here (17&0 as pair)
        ((1u << 16) | (1u <<  6)),
        ((1u << 15) | (1u <<  7)),
        ((1u << 14) | (1u <<  8) | (1u <<  9)), // three LEDs at a time here (8&9 as pair)
        ((1u << 13) | (1u << 10) | (1u <<  5)), // BUGBUG -- 5 is repeated here
        ((1u << 12) | (1u << 11)),
    };
    const uint32_t groups_center_left[]={
        (1u <<  4) | (1u <<  5),
        (1u <<  3) | (1u <<  6),
        (1u <<  2) | (1u <<  7),
        (1u <<  1) | (1u <<  8),
        (1u <<  0) | (1u <<  9),
        (1u << 17) | (1u << 10),
        (1u << 16) | (1u << 11),
        (1u << 15) | (1u << 12),
        (1u << 14) | (1u << 13),
    };
    const uint32_t groups_center_clockwise[]={
        (1u << 14) | (1u << 15),
        (1u << 16)             ,
        (1u << 17) | (1u <<  0),
        (1u <<  1) | (1u <<  2),
        (1u <<  3) | (1u <<  4),
        (1u <<  5) | (1u <<  6),
        (1u <<  7)             ,
        (1u <<  8) | (1u <<  9),
        (1u << 10) | (1u << 11),
        (1u << 12) | (1u << 13),
    };
    const uint32_t groups_top_down[]={ //MSB is last led in string...
        0b011001101011001101,
        0b100110010100110010,
        //..|...|...|...|... == 18 bits == COUNT_OF_PIXELS
    };
#endif

CPIXEL_COLOR_RGB leds[RGB_LEN]; // store as RGB as it's easier to recognize / debug in 0x00RRGGBB format
static inline void rgb_send(void){  
    for(int i=0; i<RGB_LEN; i++){
        // old code took uint32_t in format 0x00GGRRBB (GRB format), and left-shifted the value 8 bits
        // resulting in 0xGGRRBB00.  Replicate this behavior here using CPIXEL_COLOR_RGB, and let the
        // compiler optimize where possible.
        uint32_t toSend =
            (leds[i].g << 24) |
            (leds[i].r << 16) |
            (leds[i].b <<  8) ;
        pio_sm_put_blocking(pio1, 3, toSend);
    }
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
CPIXEL_COLOR_RGB color_wheel(uint8_t pos) {
  pos = 255 - pos;
  if(pos < 85) {
    return color_rgb(255 - pos * 3,             0,       pos * 3);
  } else if(pos < 170) {
    pos -= 85;
    return color_rgb(            0,       pos * 3, 255 - pos * 3);
  } else {
    pos -= 170;
    return color_rgb(      pos * 3, 255 - pos * 3,             0);
  }
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 * The ..._div() version of the function divides each of
 * the R/G/B values by the system config's led brightness.
 */
CPIXEL_COLOR_RGB color_wheel_div(uint8_t pos) {
    uint32_t bright_reduction = system_config.led_brightness;
    if (bright_reduction == 0) {
        return color_rgb(0, 0, 0);
    }
    CPIXEL_COLOR_RGB color = color_wheel(pos);
    color.r /= bright_reduction;
    color.g /= bright_reduction;
    color.b /= bright_reduction;
    return color;
}

/*
 * Assign a color to a group of LEDs.
 */
void rgb_assign_color(uint32_t index_mask, CPIXEL_COLOR_RGB rgb_color){
    for (int i=0;i<RGB_LEN; i++){
        if(index_mask&(1u<<i)) {
            leds[i]=rgb_color;
        }
    }
}

// rgb_master() is a function that can be called to animate the LEDs.
//  groups          the pixel groups; a pointer to an array of index_masks,
//                  each index_mask indicating which LEDs are considered part of the group
//  group_count     how many groups in that array
//  color_wheel     a function pointer; Function must take a single byte parameter
//                  and return a ***GRB-formatted*** color -- BUGBUG -- Verify color order that is expected?
//  color_count     the distinct seed values for color_wheel() parameter.
//                  rgb_master() will ensure the parameter is always in range [0..color_count-1]
//  color_increment the PER GROUP increment of colors for the color_wheel() parameter.
//
// returns false if additional iterations are needed to finish all cycles of the animation.
// returns true if sufficient iterations have been completed.
//
// HACKHACK -- to ensure known starting state (reset the static variables)
//             can call with group_count=0, cycles=0
bool rgb_master(
    const uint32_t *groups,
    uint8_t group_count,
    CPIXEL_COLOR_RGB (*color_wheel)(uint8_t color),
    uint8_t color_count,
    uint8_t color_increment,
    uint8_t cycles,
    uint8_t delay_ms_unused_parameter /* unused parameter ... was delay_ms ... BUGBUG: Remove unused parameter */
    ){
    static uint8_t color_idx=0; // one cycle is defined as `color_count` calls to this function
    static uint16_t completed_cycles=0;

    for(int i=0; i< group_count; i++) {
        // group_count     is uint8_t (1..255)
        // color_increment is uint8_t (0..255)
        // color_count     is uint8_t (1..255)
        // therefore, maximum value of tmp_color is
        // color_count + (group_count * color_increment) = 255 + (255 * 255) = 65010
        // This value fits in uint16_t ... but just use 32-bits.
        uint32_t tmp_color_idx = color_idx + (i*color_increment);
        tmp_color_idx %= color_count; // ensures safe to cast to uint8_t
        CPIXEL_COLOR_RGB rgb_color = color_wheel((uint8_t)tmp_color_idx);
        rgb_assign_color(groups[i], rgb_color);
    }
    rgb_send();
    ++color_idx;

    //finished one complete cycle through the colors
    if (color_idx == color_count) {
        color_idx = 0;
        ++completed_cycles;
        if (completed_cycles >= cycles) {
            // returning TRUE indicates to the caller that the animation has completed.
            // resets the cycle count to zero
            // BUGBUG - LEAVES THE color_idx AT CURRENT VALUE (?!) ... non-deterministic for caller
            completed_cycles = 0;
            // color_idx = 0
            return true;
        }
    }

    return false;
}

struct repeating_timer rgb_timer;

bool rgb_scanner(void) {
    // TODO: decode this animation and add notes on what it's intended result is
    static_assert(count_of(groups_center_left) <= (sizeof(uint16_t)*8), "uint16_t too small to hold count_of(groups_center_left) elements");
    
    // led_bitmask has a single bit set, which serves two purposes:
    // 1. when that bit reaches the LSB, it indicates the end of the animation cycle
    // 2. it selects which of the group of the pixels gets the color applied to it
    // led_bitmask is right-shifted every time this function generates a new animation frame
    static uint16_t led_bitmask = 0b1000000; // BUGBUG -- this should be:  0x01 << (count_of(groups_center_left)-1)
    // frame_delay is used to keep the current animation frame visible.
    // when it's non-zero, led_bitmask is not shifted and no changes are made to the pixel state.
    static uint8_t frame_delay_count = 0;
    static uint8_t color_idx = 0;

    static const CPIXEL_COLOR_RGB background_pixel_color = { .r = 0x0a, .g = 0x0a, .b = 0x0a };
    // each loop of the animation, use the next color in this sequence.
    // when all the colors have been used, the animation is complete.
    const CPIXEL_COLOR_RGB colors[]={
        { .r = 0xFF, .g = 0x00, .b = 0x00 },
        { .r = 0xD5, .g = 0x2A, .b = 0x00 },
        { .r = 0xAB, .g = 0x55, .b = 0x00 },
        { .r = 0xAB, .g = 0x7F, .b = 0x00 },
        { .r = 0xAB, .g = 0xAB, .b = 0x00 },
        { .r = 0x56, .g = 0xD5, .b = 0x00 },
        { .r = 0x00, .g = 0xFF, .b = 0x00 },
        { .r = 0x00, .g = 0xD5, .b = 0x2A },
        { .r = 0x00, .g = 0xAB, .b = 0x55 },
        { .r = 0x00, .g = 0x56, .b = 0xAA },
        { .r = 0x00, .g = 0x00, .b = 0xFF },
        { .r = 0x2A, .g = 0x00, .b = 0xD5 },
        { .r = 0x55, .g = 0x00, .b = 0xAB },
        { .r = 0x7F, .g = 0x00, .b = 0x81 },
        { .r = 0xAB, .g = 0x00, .b = 0x55 },
        { .r = 0xD5, .g = 0x00, .b = 0x2B },
    };

    // early exit when this animation frame should be kept as-is
    if (frame_delay_count){
        frame_delay_count--;
        return false;
    }

    // generate the next frame of the animation.
    for(int i=0; i< count_of(groups_center_left); i++){
        CPIXEL_COLOR_RGB color = background_pixel_color;
        // is this the group that gets the non-background color?
        if (led_bitmask & (1u<<i)) {
            color = colors[color_idx];
        }
        rgb_assign_color(groups_center_left[i], color);
    }
    rgb_send();

    // was this the last group to have this color applied?
    if(led_bitmask & 0b1) {

        // the final step of animation is shown 241 times (the last step of each cycle)
        frame_delay_count=0xF0;

        // HACKHACK: this INTENTIONALLY causes the first frame of the next cycle to be all-background color pixels
        //           by setting the led_bitmask bit to a bit position that will not match any of the group indices
        led_bitmask = (0x01 << (count_of(groups_center_left)));

        // use the next color reset the variables for the next cycle of the animation
        ++color_idx;

        // detect the end of the full sequence of colors...
        // BUGBUG: the animation frame for the last color is NOT shown for the full 241 frames
        if(color_idx==count_of(colors)){
            // returning TRUE indicates the full animation cycle has completed.
            color_idx=0;
            led_bitmask = 0x01 << (count_of(groups_center_left)-1);
            return true;
        }
    }else{
        frame_delay_count = 0x8; // every non-final animation frame is shown nine times
    }
    led_bitmask >>= 1;
    return false;
}

bool rgb_timer_callback(struct repeating_timer *t){
    static uint8_t mode=2;

    uint32_t color_grb;
    bool next=false;

    if(system_config.led_effect<7) { // BUGBUG -- define enum for the valid set of led effects (with appropriately descriptive names)
        mode = system_config.led_effect;
    }
    
    switch(mode) {
        case 0:; //disable
            rgb_assign_color(0xffffffff, RGBCOLOR_BLACK);
            rgb_send();  
            break;          
        case 1:;
            //solid color ... so just convert from RGB to GRB
            CPIXEL_COLOR_RGB color = rgb_from_uint32(system_config.led_color);
            rgb_assign_color(0xffffffff, color);
            rgb_send();
            break;
        case 2:;
            next = rgb_master(groups_top_left,         count_of(groups_top_left),         &color_wheel_div, 0xff, (0xff/count_of(groups_top_left)        ), 5, 10);
            break;
        case 3:;
            next = rgb_master(groups_center_left,      count_of(groups_center_left),      &color_wheel_div, 0xff, (0xff/count_of(groups_center_left)     ), 5, 10);
            break;
        case 4:;
            next = rgb_master(groups_center_clockwise, count_of(groups_center_clockwise), &color_wheel_div, 0xff, (0xff/count_of(groups_center_clockwise)), 5, 10);
            break;
        case 5:;
            next = rgb_master(groups_top_down,         count_of(groups_top_down),         &color_wheel_div, 0xff, 30, 5, 10);
            break;
        case 6:;
            next = rgb_scanner();
            break;
    }

    if(system_config.led_effect==7 && next){
        mode++;
        if(mode>6) mode=2;
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
        leds[i]= RGBCOLOR_BLACK;
    }

    // Create a repeating timer that calls repeating_timer_callback.
    // If the delay is negative (see below) then the next call to the callback will be exactly 500ms after the
    // start of the call to the last callback
    // Negative delay so means we will call repeating_timer_callback, and call it again
    // 500ms later regardless of how long the callback took to execute
    rgb_irq_enable(true);
};


void rgb_set_all(uint8_t r, uint8_t g, uint8_t b){
    uint32_t divisor = system_config.led_brightness;
    CPIXEL_COLOR_RGB color = { .r = r/divisor, .g = g/divisor, .b = b/divisor };
    rgb_assign_color(0xffffffff, color);
    rgb_send();
}

//function to control LED from led mode onboard demo
#define DEMO_LED 1
void rgb_put(uint32_t color) // external storage is 0x00RRGGBB format
{
    // first set each pixel to "off"
    for (int i=0;i<RGB_LEN; i++){
        leds[i] = RGBCOLOR_BLACK;
    }
    // then set the single demo pixel to the desired color
    leds[DEMO_LED]= rgb_from_uint32(color);
    rgb_send();
};