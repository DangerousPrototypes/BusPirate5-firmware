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
    static const uint8_t COUNT_OF_PIXELS = 16u;

    const uint32_t groups_top_left[] = {
        ((1u <<  1) | (1u <<  2)             ),
        ((1u <<  0) | (1u <<  3)             ),
        ((1u <<  4) | (1u <<  5) | (1u << 15)),
        ((1u <<  6) | (1u <<  7) | (1u << 14)),
        ((1u <<  8) | (1u << 13)             ),
        ((1u <<  9) | (1u << 12)             ),
        ((1u << 10) | (1u << 11)             ),
    };
    const uint32_t groups_center_left[] = {
        ((1u <<  3) | (1u <<  4)                         ),
        ((1u <<  2) | (1u <<  5)                         ),
        ((1u <<  1) | (1u <<  6)                         ),
        ((1u <<  0) | (1u <<  7) | (1u <<  8) | (1 <<  9)),
        ((1u << 10) | (1u << 15)                         ),
        ((1u << 11) | (1u << 14)                         ),
        ((1u << 12) | (1u << 13)                         ),
    };  
    const uint32_t groups_center_clockwise[] = {
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
    const uint32_t groups_top_down[] = {
        0b0011001010011001,
        0b1100110101100110,
    }; //MSB is last led in string...
#elif BP5_REV >= 10
    static const uint8_t COUNT_OF_PIXELS = 18u;

    const uint32_t groups_top_left[] = {
        ((1u <<  2)   |              (1u <<  3)),
        ((1u <<  1)   |              (1u <<  4)),
        ((1u <<  0)   | (1u << 17) | (1u <<  5)),
        ((1u << 16)   |              (1u <<  6)),
        ((1u << 15)   |              (1u <<  7)),
        ((1u << 14)   | (1u <<  9) | (1u <<  8)),
        ((1u << 13)   | (1u << 10) | (1u <<  5)), // BUGBUG -- LED 5 is likely a typo?
        ((1u << 12)   | (1u << 11)             ),
    };
    const uint32_t groups_center_left[] = {
        ((1u <<  4) | (1u <<  5)),
        ((1u <<  3) | (1u <<  6)),
        ((1u <<  2) | (1u <<  7)),
        ((1u <<  1) | (1u <<  8)),
        ((1u <<  0) | (1u <<  9)),
        ((1u << 17) | (1u << 10)),
        ((1u << 16) | (1u << 11)),
        ((1u << 15) | (1u << 12)),
        ((1u << 14) | (1u << 13)),
    };  
    const uint32_t groups_center_clockwise[] = {
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
    };
    const uint32_t groups_top_down[] = {
        0b011001101011001101,
        0b100110010100110010
    }; //MSB is last led in string...
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


void rgb_assign_color(uint32_t index_mask, uint32_t color){
    for (int i=0;i<RGB_LEN; i++){
        if(index_mask&(1u<<i)) {
            leds[i]=color;
        }
    }
}

//something like this to cycle, delay, return done
bool rgb_master(const uint32_t *groups, uint8_t group_count, uint32_t (*color_wheel)(uint8_t color), uint8_t color_count, uint8_t color_increment, uint8_t cycles, uint8_t delay_ms ){
    static uint8_t color=0;
    static uint16_t c=0;

    for(int i=0; i< group_count; i++){
        rgb_assign_color(groups[i], color_wheel( (color+(i*color_increment))) );
    }
    rgb_send();

    //finished one complete cycle
    if((color==color_count)){
        color=0;
        c++;
        if(c==cycles){
            c=0;
            return true;
        }        
    }

    color+=1;
    return false;
}

struct repeating_timer rgb_timer;

bool rgb_scanner(void){
    static uint16_t bitmask=0b1000000;
    static uint8_t delay=0;
    static uint8_t color=0;
    
    const uint32_t colors[]={
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
    0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
    0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
    0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B
    };

    if(delay){
        delay--;
        return false;
    }
    uint32_t color_grb=((((colors[color]&0xff0000)/system_config.led_brightness)&0xff0000)>>8);
    color_grb|=((((colors[color]&0x00ff00)/system_config.led_brightness)&0x00ff00)<<8);
    color_grb|=((((colors[color]&0x0000ff)/system_config.led_brightness)&0x0000ff));
    for(int i=0; i< count_of(groups_center_left); i++){
        rgb_assign_color(groups_center_left[i], (bitmask & (1u<<i))?color_grb:0x0a0a0a);
    }   
    rgb_send();
    
    if(bitmask & 0b1){
        delay=0xF0;
        bitmask=(0x01 << (count_of(groups_center_left)));
        color++;
        if(color==count_of(colors)){
            color=0;
            bitmask=bitmask>>1;
            return true;
        }
    }else{
        delay=0x8;
    }
    bitmask=bitmask>>1;
    return false;
}

bool rgb_timer_callback(struct repeating_timer *t){
    static uint8_t mode=2;

    //shortened list of HSV colors from fastLED
    const uint32_t colors[]={0xFF0000, 0xD52A00, 0xAB7F00,0x00FF00, 0x0000FF, 0x5500AB, 0xAB0055};


    uint32_t color_grb;
    bool next=false;

    if(system_config.led_effect<7) {
        mode = system_config.led_effect;
    }
    
    switch(mode) {
        case 0: //disable
            rgb_assign_color(0xffffffff, 0x000000);
            rgb_send();  
            break;          
        case 1:
            //solid
            // NOTE: swaps from RGB to GRB because that is what the LED strip uses
            color_grb =((((colors[system_config.led_color]&0xff0000)/system_config.led_brightness)&0xff0000)>>8);
            color_grb|=((((colors[system_config.led_color]&0x00ff00)/system_config.led_brightness)&0x00ff00)<<8);
            color_grb|=((((colors[system_config.led_color]&0x0000ff)/system_config.led_brightness)&0x0000ff));            
            rgb_assign_color(0xffffffff, color_grb);
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
            next = rgb_master(groups_top_down,         count_of(groups_top_down),         &color_wheel_div, 0xff, (                                    30), 5, 10);
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
    rgb_assign_color(0xffffffff, color);
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