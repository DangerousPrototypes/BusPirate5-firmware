#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"
#include "system_config.h"
#include "pirate/rgb.h"


//        REV10                     REV8             
//                                                    
//    11 10  9  8  7            10  9  8  7  6        
// 12    +-------+    6     11    +-------+     5    
// 13    |       |    5     12    |       |     4    
// USB   | OLED  |   []     USB   | OLED  |    []    
// 14    |       |    4     13    |       |     3    
// 15    +-------+    3     14    +-------+     2    
//    16 17  0  1  2            15  x  x  0  1       
//
#define COUNT_OF_PIXELS RGB_LEN // 18 for Rev10, 16 for Rev8

static PIO pio;
static int sm;
static uint offset;

#pragma region    // 8-bit scaled pixel coordinates and angle256
    /// @brief Scaled coordinates in range [0..255]
    typedef struct _coordin8 {
        uint8_t x;
        uint8_t y;
    } coordin8_t;


    /// @brief Each pixel's coordinate in a 256x256 grid, as
    ///        extracted from the PCB layout and Pick'n'Place data.
    /// @details The grid is oriented with the origin (0,0) at the
    ///          upper left, similar to a PC screen.  Orient the PCB
    ///          so the USB port is on the left, and the
    ///          plank connector is on the right.
    ///
    ///            y
    ///          x +---------------> +x
    ///            |    11  10 ...
    ///            |  12
    ///            |  ...
    ///            V
    ///           +y
    ///
    static const coordin8_t pixel_coordin8[] = {
        //                        // SIDE      POSITION    FACING
        // clang-format off
        #if BP5_REV >= 10
        { .x = 127, .y = 255,  }, // bottom    center      out
        #endif
        { .x = 165, .y = 255,  }, // bottom    right       side
        { .x = 202, .y = 255,  }, // bottom    right       out
        { .x = 255, .y = 202,  }, // right     bottom      out
        { .x = 255, .y = 171,  }, // right     bottom      side    (by plank connector)
        { .x = 255, .y =  84,  }, // right     top         side    (by plank connector)
        { .x = 255, .y =  52,  }, // right     top         out
        { .x = 202, .y =   0,  }, // top       right       out
        { .x = 165, .y =   0,  }, // top       right       side
        { .x = 127, .y =   0,  }, // top       center      out
        { .x =  90, .y =   0,  }, // top       left        side
        { .x =  52, .y =   0,  }, // top       left        out
        { .x =   0, .y =  52,  }, // left      top         out
        { .x =   0, .y =  84,  }, // left      top         side    (by USB port)
        { .x =   0, .y = 171,  }, // left      bottom      side    (by USB port)
        { .x =   0, .y = 202,  }, // left      bottom      out
        { .x =  52, .y = 255,  }, // bottom    left        out
        #if BP5_REV >= 10
        { .x =  90, .y = 255,  }, // bottom    left        side
        #endif
        // clang-format on
    };

    /// @brief Angular position in 1/256th-circle units, as
    ///        extracted from the PCB layout and Pick'n'Place data.
    ///        From the center of the PCB, the zero angle is
    ///        directly towards the center of the plank connector,
    ///        with angles increasing in the anti-clockwise direction.
    static const uint8_t pixel_angle256[] = {
        //                  // SIDE      POSITION    FACING
        // clang-format off
        #if BP5_REV >= 10
        192,                // bottom    center      out
        #endif
        204,                // bottom    right       side
        214,                // bottom    right       out
        234,                // right     bottom      out
        243,                // right     bottom      side    (by plank connector)
         13,                // right     top         side    (by plank connector)
         22,                // right     top         out
         42,                // top       right       out
         52,                // top       right       side
         64,                // top       center      out
         76,                // top       left        side
         86,                // top       left        out
        106,                // left      top         out
        115,                // left      top         side    (by USB port)
        141,                // left      bottom      side    (by USB port)
        150,                // left      bottom      out
        170,                // bottom    left        out
        #if BP5_REV >= 10
        180,                // bottom    left        side
        #endif
        // clang-format on
    };

    static_assert(count_of(pixel_coordin8) == COUNT_OF_PIXELS);
    static_assert(count_of(pixel_angle256) == COUNT_OF_PIXELS);

    // Sadly, C still refuses to allow the following format in static_assert(), saying it's not constant.
    //static const uint32_t PIXEL_MASK_UPPER = 0b0....1;
    //static const uint32_t PIXEL_MASK_SIDE  = 0b1....0;

    // clang-format off
    #if BP5_REV <= 9
        // Pixels that shine    orthogonal to OLED: idx     1,2,    5,6,  8,  10,11,      14,15,
        #define PIXEL_MASK_UPPER ( 0b1100110101100110 )
        // Pixels that shine    orthogonal to OLED: idx   0,    3,4,    7,  9,      12,13,
        #define PIXEL_MASK_SIDE  ( 0b0011001010011001 )
    #else
        // Pixels that shine in direction  of OLED: idx 0,  2,3,    6,7,  9,   11,12,      15,16
        #define PIXEL_MASK_UPPER (0b011001101011001101)
        // Pixels that shine    orthogonal to OLED: idx   1,    4,5,    8,  10,      13,14,     17
        #define PIXEL_MASK_SIDE  (0b100110010100110010)
    #endif
    // clang-format on

    static const uint32_t groups_top_down[] = {
        PIXEL_MASK_UPPER,
        PIXEL_MASK_SIDE,
    }; // MSb is last led in string...
    #define PIXEL_MASK_ALL ((1u << COUNT_OF_PIXELS) - 1)
    static_assert(COUNT_OF_PIXELS < (sizeof(uint32_t)*8)-1, "Too many pixels for pixel mask definition to be valid");
    static_assert((PIXEL_MASK_UPPER & PIXEL_MASK_SIDE) == 0, "Pixel cannot be both upper and side");
    static_assert((PIXEL_MASK_UPPER | PIXEL_MASK_SIDE) == PIXEL_MASK_ALL, "Pixel must be either upper or side");


#pragma endregion

// Externally to this file, colors are always handled as uint32_t values
// which stores the color in 0x00RRGGBB format.
// Internally to this file, the color is stored in the CPIXEL_COLOR format.
// The only location where the order of bytes changes is when actually
// pushing the bytes to PIO ... to match the color format expected by the
// particular type of pixels used:  See update_pixels().

typedef struct _CPIXEL_COLOR {
    // pad to 32-bits to ensure compiler can optimize away conversion
    // from constexpr uint32_t colors (0x00RRGGBB format) into this
    // type into a noop (same underlying data in little-endian)
    union {
        uint8_t blue;
        uint8_t b;
    };
    union {
        uint8_t green;
        uint8_t g;
    };
    union {
        uint8_t red;
        uint8_t r;
    };
    uint8_t _unused;
} CPIXEL_COLOR;

CPIXEL_COLOR pixels[COUNT_OF_PIXELS]; // store as RGB ... as it's the common format

// C23 would allow this to be `constexpr`
// here, `static const` relies on compiler to optimize this away to a literal `((uint32_t)0u)`
static const CPIXEL_COLOR PIXEL_COLOR_BLACK = { .r=0x00, .g=0x00, .b=0x00 };

static CPIXEL_COLOR color_from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    CPIXEL_COLOR c = { .r = r, .g = g, .b = b };
    return c;
}
static CPIXEL_COLOR color_from_uint32(uint32_t c) {
    CPIXEL_COLOR result = {
        ._unused = (c >> 24) & 0xff, // carry the input data, to allow compiler to optimization this to a noop
        .r =       (c >> 16) & 0xff,
        .g =       (c >>  8) & 0xff,
        .b =       (c >>  0) & 0xff,
    };
    return result;
}
static uint32_t color_as_uint32(CPIXEL_COLOR c) {
    return (c.r << 16) | (c.g << 8) | c.b;
}
static CPIXEL_COLOR reduce_brightness(CPIXEL_COLOR c, uint8_t numerator, uint8_t divisor) {
    CPIXEL_COLOR r = {
        .r = c.r * numerator / divisor,
        .g = c.g * numerator / divisor,
        .b = c.b * numerator / divisor,
    };
    return r;
}



#pragma region    // Legacy pixel animation groups

    // Note that both the layout and overall count of pixels
    // has changed between revisions.  As a result, the count
    // of elements for any of these arrays may differ.
    //
    // groups_top_left[]:
    //    generally start at the top left corner of the device,
    //    and continue in a diagnol wipe to bottom right.
    //
    // groups_center_left[]:
    //    generally start at left center two pixels (by USB port),
    //    each of which flows towards the plank connector (in opposite directions)
    //
    // groups_center_clockwise[]:
    //    Similar to a clock, rotating around the device clockwise.
    //
    // groups_top_down[]:
    //    All the pixels facing upwards as one group, and all the pixels
    //    facing the sides as a second group.

    #if BP5_REV <= 9
        static const uint32_t groups_top_left[] = {
            // clang-format off
            ((1u <<  1) | (1u <<  2)             ),
            ((1u <<  0) | (1u <<  3)             ),
            ((1u << 15) | (1u <<  4) | (1u <<  5)), // pair up 4/5
            ((1u << 14) | (1u <<  6) | (1u <<  7)), // pair up 6/7
            ((1u << 13) | (1u <<  8)             ),
            ((1u << 12) | (1u <<  9)             ),
            ((1u << 11) | (1u << 10)             ),
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
        static const uint32_t groups_top_left[] = {
            // TODO: use grid mappings instead
            //       e.g., for iteration target from 255..0
            //             (x+y)/2 == iteration
            // clang-format off
            (               (1u <<  2) | (1u <<  3)),
            (               (1u <<  1) | (1u <<  4)),
            ((1u << 17)   | (1u <<  0) | (1u <<  5)),
            ((1u << 16)   |              (1u <<  6)),
            ((1u << 15)   |              (1u <<  7)),
            ((1u << 14)   | (1u <<  9) | (1u <<  8)),
            ((1u << 13)   | (1u << 10)             ),
            ((1u << 12)   | (1u << 11)             ),
            // clang-format on
        };
        static const uint32_t groups_center_left[] = {
            // TODO: use angular mappings instead
            //       e.g., for iteration target from 255..0
            //             // convert to angular range: [0..127] based on absolute offset from angle 0
            //             uint8_t pix_a = (a256 > 127u) ? 256u - a256 : a256;
            //             // scale it up to range [0..255]
            //             pix_a *= 2u;
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
            // TODO: use angular mappings instead
            //       e.g., for iteration target from 255..0
            //             uint8_t pix_a = 256u - a256;
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
#pragma endregion // Legacy pixel animation groups


static inline void update_pixels(void) {  
    for (int i = 0; i < COUNT_OF_PIXELS; i++) {

        // little-endian, so 0x00GGRRBB  is stored as 0xBB 0xRR 0xGG 0x00
        // Shifting it left by 8 bits will give bytes 0x00 0xBB 0xRR 0xGG
        // which allows the PIO to unshift the bytes in the correct order
        CPIXEL_COLOR c = pixels[i];
        c.r = c.r / system_config.led_brightness_divisor;
        c.g = c.g / system_config.led_brightness_divisor;
        c.b = c.b / system_config.led_brightness_divisor;
        uint32_t toSend =
            (c.g << 24) |
            (c.r << 16) |
            (c.b <<  8) ;

        // TODO: define symbolic constant for which PIO / state machine (no magic numbers!)
        //       e.g., #define WS2812_PIO  pio1
        //       e.g., #define WS2812_SM   3
        pio_sm_put_blocking(pio, sm, toSend);
    }
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
static CPIXEL_COLOR color_wheel(uint8_t pos) {
    uint8_t r, g, b;

    pos = 255 - pos;
    if(pos < 85) {
        r = 255u - (pos * 3u);
        g = 0u;
        b = pos * 3;
    } else if(pos < 170) {
        pos -= 85;
        r = 0u;
        g = pos * 3u;
        b = 255u - (pos * 3u);
    } else {
        pos -= 170;
        r = pos * 3u;
        g = 255u - (pos * 3u);
        b = 0u;
    }
    return color_from_rgb(r, g, b);
}

static void assign_pixel_color(uint32_t index_mask, CPIXEL_COLOR pixel_color){
    for (int i = 0; i < COUNT_OF_PIXELS; i++){
        if (index_mask & (1u << i)) {
            pixels[i] = pixel_color;
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
static bool rgb_master(
    const uint32_t *groups,
    uint8_t group_count,
    CPIXEL_COLOR (*color_wheel)(uint8_t color),
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

        CPIXEL_COLOR rgb_color = color_wheel((uint8_t)tmp_color_idx);
        assign_pixel_color(groups[i], rgb_color);
    }
    update_pixels();
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

static bool animation_gentle_glow(void) {

    static const uint16_t animation_cycles = 240 * 16 + 9 * 9; // approximately same time as scanner
    static uint16_t cycle_count = 0;
    // clang-format off
    CPIXEL_COLOR top_color  = { .r = 15, .g = 15, .b = 15 };
    CPIXEL_COLOR side_color = { .r = 60, .g = 60, .b = 60 };
    assign_pixel_color(PIXEL_MASK_UPPER, top_color );
    assign_pixel_color(PIXEL_MASK_SIDE,  side_color);
    // clang-format on
    update_pixels();

    ++cycle_count;
    if (cycle_count >= animation_cycles) {
        cycle_count = 0;
        return true;
    }
    return false;
}

/// @brief angular wipe from upper left to bottom right, with diffusion / blur
/// @param color The color to use for the wipe, or RGBCOLOR_BLACK to use angle-based rainbow
/// @return true when full animation has run its course
static bool animation_angular_wipe(CPIXEL_COLOR color) {
    static const uint16_t value_diffusion = 40u;
    static const uint16_t starting_value = 0u;
    static const uint16_t ending_value = value_diffusion*4u + 256u;
    static const uint16_t default_frame_delay = 1u;

    static uint8_t frame_delay_count = 0;
    static uint16_t current_value = starting_value;

    // delay with each frame showing for multiple callbacks
    if (frame_delay_count != 0) {
        --frame_delay_count;
        return false;
    }

    // ending condition
    if (current_value == ending_value) {
        frame_delay_count = default_frame_delay * 4;
        current_value = starting_value;
        return true;
    }

    // what color to use for this value?
    if (color.r == 0 && color.g == 0 && color.b == 0) {
        color = color_wheel(current_value);
    }

    // always loop through each pixel
    for (int i = 0; i < count_of(pixel_coordin8); ++i) {
        // does it get color for this value?

        // TODO: allow arbitrary angle for the wipe.
        // For now, hard-coded to (X+Y)/2 for wipe
        // from upper left to lower right corner ...
        uint16_t pix_v = (((uint16_t)pixel_coordin8[i].x) + ((uint16_t)pixel_coordin8[i].y)) / 2u;
        
        // shift from [0..255] to [2*value_diffusion .. 255 + 4*value_diffusion]
        // this allows smooth entry and exit diffusion effects
        pix_v += 2u * value_diffusion;

        // diff is not a true cartesion distance,
        // but good enough for current purposes.
        uint16_t diff = abs(current_value - pix_v);

        // until migrate the FastLED color mixing,
        // background must be black for blur / diffusion effects
        if (diff < value_diffusion) {
            static_assert(value_diffusion < 256u, "invalid brightness calculations when value_diffusion is too large (limited to uint8_t)");
            CPIXEL_COLOR dimmed_color = reduce_brightness(color, value_diffusion - diff, value_diffusion);
            assign_pixel_color(1u << i, dimmed_color);
        } else {
            assign_pixel_color(1u << i, PIXEL_COLOR_BLACK);
        }
    }
    update_pixels();

    // same delay for all frames
    frame_delay_count = default_frame_delay;
    current_value++; // mod(256) is implicit
    return false;
}

/// @brief  starting at noon/up, color pixels in an anti-clockwise direction
/// @param color color to use for the pixels, or RGBCOLOR_BLACK to use angle-based rainbow
/// @return true when rull animation has run its course
static bool animation_anticlockwise_scan(CPIXEL_COLOR color) {
    static const uint8_t starting_angle256 = 64u;
    static const uint8_t angle_diffusion = 20u;
    static const uint8_t default_frame_delay = 3u;

    static bool first_time_through = true;
    static uint8_t frame_delay_count = 0;
    static uint8_t current_angle256 = starting_angle256;

    // delay with each frame showing for multiple callbacks
    if (frame_delay_count != 0) {
        --frame_delay_count;
        return false;
    }

    // ending condition
    if (current_angle256 == starting_angle256) {
        if (first_time_through) {
            first_time_through = false;
        } else {
            first_time_through = true;
            return true;
        }
    }

    // what color to use for this angle?
    if (color.r == 0 && color.g == 0 && color.b == 0) {
        color = color_wheel(current_angle256);
    }

    // always loop through each pixel
    for (int i = 0; i < count_of(pixel_angle256); ++i) {
        // does it get color at this angle?
        uint8_t pixel_a = pixel_angle256[i];
        uint8_t angle_diff = abs(current_angle256 - pixel_a);
        if (angle_diff < angle_diffusion) {
            CPIXEL_COLOR reduced = reduce_brightness(color, angle_diffusion - angle_diff, angle_diffusion);
            assign_pixel_color(1u << i, reduced);
        } else {
            assign_pixel_color(1u << i, PIXEL_COLOR_BLACK);
        }
    }
    update_pixels();

    // same delay for all frames
    frame_delay_count = default_frame_delay;
    current_angle256++; // mod(256) is implicit
    return false;
}

/// @brief  starting at two pixels near USB port, scan edges towards plank connector
/// @param color color to use for the pixels, or RGBCOLOR_BLACK to use angle-based rainbow
/// @return true when rull animation has run its course
static bool animation_scanner(CPIXEL_COLOR color) {
    // TODO: implement this function
    return true;
}

static bool rgb_scanner(void) {

    static_assert(count_of(groups_center_left) < (sizeof(uint16_t)*8), "uint16_t too small to hold count_of(groups_center_left) elements");

    // pixel_bitmask has a single bit set, which serves two purposes:
    // 1. when that bit shifts off the LSB, it indicates the end of the animation cycle
    // 2. the mask selects which of the group of the pixels gets the color applied this round
    // pixel_bitmask is right-shifted every time this function generates a new animation frame
    // (note: not every time the function is called ... no shift if still in a delay cycle)
    static uint16_t pixel_bitmask = 1u << (count_of(groups_center_left) - 1);
    static uint8_t frame_delay_count = 0u;
    static uint8_t color_idx = 0;
    
    static const CPIXEL_COLOR background_pixel_color = { .r = 0x20, .g = 0x20, .b = 0x20 };
    // each loop of the animation, use the next color in this sequence.
    // when all the colors have been used, the animation is complete.
    const CPIXEL_COLOR colors[]={
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

    // early exit from function when keeping current animation frame
    if (frame_delay_count) {
        --frame_delay_count;
        return false;
    }

    // generate the next frame of the animation
    for (int i = 0; i < count_of(groups_center_left); i++) {
        CPIXEL_COLOR color = background_pixel_color;
        // does this pixel get the non-background color?
        if (pixel_bitmask & (1u << i)) {
            color = colors[color_idx];
        }
        assign_pixel_color(groups_center_left[i], color);
    }
    update_pixels();
    
    // was this the last group to get the current color applied?
    if (pixel_bitmask == 0) {
        // set a longer delay before starting the next color
        frame_delay_count = 0xF0;
        // reset the pixel bitmask and move to the next color
        pixel_bitmask = (0x01 << (count_of(groups_center_left)));
        color_idx++;
        // however, if there are no more colors,
        // then re-initialize static variables (reset state)
        // and return true to indicate the animation is complete.
        if (color_idx == count_of(colors)) {
            color_idx = 0;
            pixel_bitmask = pixel_bitmask >> 1;
            return true;
        }
    } else {
        // stay on each frame for eight additional calls to this fn
        frame_delay_count = 0x8;
    }
    pixel_bitmask = pixel_bitmask >> 1;
    return false;
}

static bool pixel_timer_callback(struct repeating_timer *t){
    static uint8_t mode=2;

    uint32_t color_grb;
    bool next=false;

    if (system_config.led_effect < MAX_LED_EFFECT) {
        mode = system_config.led_effect;
    }
    
    // clang-format off
    switch(mode) {
        case LED_EFFECT_DISABLED:
            assign_pixel_color(PIXEL_MASK_ALL, PIXEL_COLOR_BLACK);
            update_pixels();  
            break;          
        case LED_EFFECT_SOLID:; // semicolon is required for ... reasons
            CPIXEL_COLOR color = color_from_uint32(system_config.led_color);
            assign_pixel_color(PIXEL_MASK_ALL, color);
            update_pixels();
            break;
        case LED_EFFECT_ANGLE_WIPE:
            next = rgb_master(groups_top_left,         count_of(groups_top_left),         &color_wheel, 0xff, (0xff/count_of(groups_top_left)        ), 5, 10);
            break;
        case LED_EFFECT_CENTER_WIPE:
            next = rgb_master(groups_center_left,      count_of(groups_center_left),      &color_wheel, 0xff, (0xff/count_of(groups_center_left)     ), 5, 10);
            break;
        case LED_EFFECT_CLOCKWISE_WIPE:
            next = rgb_master(groups_center_clockwise, count_of(groups_center_clockwise), &color_wheel, 0xff, (0xff/count_of(groups_center_clockwise)), 5, 10);
            break;
        case LED_EFFECT_TOP_SIDE_WIPE:
            next = rgb_master(groups_top_down,         count_of(groups_top_down),         &color_wheel, 0xff, (                                    30), 5, 10);
            break;
        case LED_EFFECT_SCANNER:
            //static const CPIXEL_COLOR scanner_color = { .r = 0x80, .g = 0x20, .b = 0x20 };
            //next = animation_angular_wipe(scanner_color);
            //next = animation_anticlockwise_scan(scanner_color);
            next = rgb_scanner();
            break;
        case LED_EFFECT_GENTLE_GLOW:
            next = animation_gentle_glow();
            break;
        case LED_EFFECT_PARTY_MODE:
            assert(!"Party mode should never be value of the *local* variable!");
            break;
    }
    // clang-format on

    if (system_config.led_effect == LED_EFFECT_PARTY_MODE && next) {
        static_assert(LED_EFFECT_DISABLED == 0, "LED_EFFECT_DISABLED must be zero");
        static_assert(MAX_LED_EFFECT-1 == LED_EFFECT_PARTY_MODE, "LED_EFFECT_PARTY_MODE must be the last effect");
        ++mode;
        if (mode >= LED_EFFECT_PARTY_MODE) {
            mode = LED_EFFECT_DISABLED + 1;
        }
    }

    return true;
}



// ================================================================================
// Exported functions (Pixel API) follows:


void rgb_irq_enable(bool enable){
    static bool enabled=false;
    if(enable && !enabled)
    {
        add_repeating_timer_ms(-10, pixel_timer_callback, NULL, &rgb_timer);
        enabled=true;
    }
    else if(!enable && enabled)
    {
        cancel_repeating_timer(&rgb_timer);
        enabled = false;
    }
}

void rgb_init(void){

    #if (BP_VER == 6)
        bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, RGB_CDO, 16, true);
    #else
        bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, RGB_CDO, 1, true);
    #endif
    hard_assert(success);
    
    ws2812_program_init(pio, sm, offset, RGB_CDO, 800000, false);

    for (int i = 0; i < COUNT_OF_PIXELS; i++){
        pixels[i] = PIXEL_COLOR_BLACK;
    }

    // Create a repeating timer that calls repeating_timer_callback.
    // If the delay is negative (see below) then the next call to the callback will be exactly 500ms after the
    // start of the call to the last callback
    // Negative delay so means we will call repeating_timer_callback, and call it again
    // 500ms later regardless of how long the callback took to execute
    rgb_irq_enable(true);
};


void rgb_set_all(uint8_t r, uint8_t g, uint8_t b){
    CPIXEL_COLOR color = { .r = r, .g = g, .b = b };
    assign_pixel_color(PIXEL_MASK_ALL, color);
    update_pixels();
}

//function to control LED from led mode onboard demo
#define DEMO_LED 1
void rgb_put(uint32_t color)
{
    // first set each pixel to off
    for (int i = 0; i < COUNT_OF_PIXELS; i++){
        pixels[i] = PIXEL_COLOR_BLACK;
    }
    pixels[DEMO_LED] = color_from_uint32(color);
    update_pixels();
};
