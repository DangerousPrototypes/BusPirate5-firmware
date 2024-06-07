#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"
#include "system_config.h"

#define RGB_MAX_BRIGHT 32

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

uint32_t leds[RGB_LEN];

struct rgb_segment{
    uint32_t speed;
    uint32_t increment;
    bool direction;
    uint32_t destination;
    uint32_t fade;
    uint8_t led_total;
    uint8_t led_position;   
};

struct rgb_program{
    struct rgb_segment segment;
    bool (*handler)(struct rgb_segment *segment, uint8_t led);
};

struct rgb_program rgb_handlers[RGB_LEN];

static inline void rgb_send(void){  
    for(int i=0; i<RGB_LEN; i++){
        pio_sm_put_blocking(pio1, 3, ((leds[i]) << 8u));        
    }
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r) << 8) |
         ((uint32_t)(g) << 16) |
         (uint32_t)(b);
}


void irq_rgb(void)
{
    for(uint8_t i =0; i<RGB_LEN; i++)
    {
        if(rgb_handlers[i].handler!=0)
        {
            rgb_handlers[i].handler(&rgb_handlers[i].segment, i);
        }
    }    
    rgb_send();
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

    return ((g/system_config.led_brightness)<<16) | ((r/system_config.led_brightness)<<8) | (b/system_config.led_brightness);
}

// BUGBUG -- Many of the callers convert an RGB color to GRB before passing
//           it to this function.  This means the color parameter is GRB (not RGB)?
// TODO: define RGB and GRB structures, and use them for clarity rather than uint32_t
void rgb_assign_grb_color(uint32_t index_mask, uint32_t grb_color){
    for (int i=0;i<RGB_LEN; i++){
        if(index_mask&(1u<<i)) {
            leds[i]=grb_color;
        }
    }
}

//something like this to cycle, delay, return done
//This needs some more documentation:
//  groups          the pixel groups; a pointer to an array of index_masks,
//                  each index_mask indicating which LEDs are considered part of the group
//  group_count     how many groups in that array
//  color_wheel     a function pointer; Function must take a single byte parameter
//                  and return a ***GRB-formatted*** color -- BUGBUG -- Verify color order that is expected?
//  color_count     the distinct seed values for color_wheel() parameter.
//                  rgb_master() will ensure the parameter is always in range [0..color_count-1]
//  color_increment the PER GROUP increment of colors for the color_wheel() parameter.
//returns false if additional iterations are needed.
//returns true if sufficient iterations have been completed.
//
// HACKHACK -- to ensure known starting state (reset the static variables)
//             can call with group_count=0, cycles=0
bool rgb_master(
    const uint32_t *groups,
    uint8_t group_count,
    uint32_t (*color_wheel)(uint8_t color),
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
        uint32_t rgb_color = color_wheel((uint8_t)tmp_color_idx);
        rgb_assign_grb_color(groups[i], rgb_color);
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
    
    // led_bitmask has two purposes:
    // 1. it
    static uint16_t led_bitmask = 0b1000000; // BUGBUG -- should this be (0x01 << (count_of(groups_center_left)))?
    static uint8_t frame_delay_count = 0;
    static uint8_t color_idx = 0;
    
    const uint32_t colors[]={
        0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
        0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
        0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
        0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B
    };

    if (frame_delay_count){
        frame_delay_count--;
        return false;
    }
    uint32_t color_grb = 0; // swap from RGB to GRB
    color_grb |= (( ((colors[color_idx] & 0xff0000) / system_config.led_brightness) & 0xff0000) >> 8);
    color_grb |= (( ((colors[color_idx] & 0x00ff00) / system_config.led_brightness) & 0x00ff00) << 8);
    color_grb |= (( ((colors[color_idx] & 0x0000ff) / system_config.led_brightness) & 0x0000ff)     );
    for(int i=0; i< count_of(groups_center_left); i++){
        rgb_assign_grb_color(groups_center_left[i], (led_bitmask & (1u<<i)) ? color_grb : 0x0a0a0a);
    }   
    rgb_send();
    
    // led_bitmask has two purposes:
    // this detects the end of one cycle of animation

    if(led_bitmask & 0b1) {
        frame_delay_count=0xF0; // the final step of animation is shown 241 times (the last step of each cycle)
        led_bitmask = (0x01 << (count_of(groups_center_left)));
        color_idx++;
        if(color_idx==count_of(colors)){
            color_idx=0;
            led_bitmask >>= 1;
            return true;
        }
    }else{
        frame_delay_count = 0x8; // every non-final step of the animation is shown nine times
    }
    led_bitmask >>= 1;
    return false;
}

bool rgb_timer_callback(struct repeating_timer *t){
    static uint8_t mode=2;

    uint32_t color_grb;
    bool next=false;

    if(system_config.led_effect<7) {
        mode = system_config.led_effect;
    }
    
    switch(mode) {
        case 0: //disable
            rgb_assign_grb_color(0xffffffff, 0x000000);
            rgb_send();  
            break;          
        case 1:
            //solid color ... so just convert from RGB to GRB
            color_grb  = (( ((system_config.led_color & 0xff0000) / system_config.led_brightness) & 0xff0000) >> 8);
            color_grb |= (( ((system_config.led_color & 0x00ff00) / system_config.led_brightness) & 0x00ff00) << 8);
            color_grb |= (( ((system_config.led_color & 0x0000ff) / system_config.led_brightness) & 0x0000ff)     );
            rgb_assign_grb_color(0xffffffff, color_grb);
            rgb_send();
            break;
        case 2:
            next = rgb_master(groups_top_left,         count_of(groups_top_left),         &color_wheel_div, 0xff, (0xff/count_of(groups_top_left)        ), 5, 10);
            break;
        case 3:
            next = rgb_master(groups_center_left,      count_of(groups_center_left),      &color_wheel_div, 0xff, (0xff/count_of(groups_center_left)     ), 5, 10);
            break;
        case 4:
            next = rgb_master(groups_center_clockwise, count_of(groups_center_clockwise), &color_wheel_div, 0xff, (0xff/count_of(groups_center_clockwise)), 5, 10);
            break;
        case 5:
            next = rgb_master(groups_top_down,         count_of(groups_top_down),         &color_wheel_div, 0xff, 30, 5, 10);
            break;
        case 6:
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
    uint32_t color= ((g/system_config.led_brightness)<<16) | ((r/system_config.led_brightness)<<8) | (b/system_config.led_brightness);
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