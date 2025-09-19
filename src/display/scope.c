//
//	(C) Paul Campbell, Moonbase Otago 2023 paul@taniwha.com
//
//  MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the “Software”), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial
// portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mem.h"
#include "system_config.h"
#include "command_struct.h"
#include "hardware/uart.h"
// #include "hiz.h"
#include "pirate/bio.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "font/font.h"
// #include "font/hunter-23pt-24h24w.h"
// #include "font/hunter-20pt-21h21w.h"
// #include "font/hunter-14pt-19h15w.h"
// #include "font/hunter-12pt-16h13w.h"
// #include "font/background.h"
// #include "font/background_image_v4.h"
extern const FONT_INFO hunter_12ptFontInfo;
#include "ui/ui_flags.h"
#include "ui/ui_lcd.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_cmdln.h"
#include "usb_rx.h"
#include "pirate/amux.h"

static int convert_trigger_position(int pos);

#define VS 240
#define HS 320
const uint32_t V5 = 0x0c05; // 5V

#define CAPTURE_DEPTH 64
#define BUFFERS (5 * 10 * 2 + 1) // 320x10 = standard samples rate (10 samples/pixel) 4x screen width

#define MALLOC_SIZE (2 * 2 * BUFFERS * CAPTURE_DEPTH) + (VS * HS / 2)
static volatile uint32_t sample_first = 0, sample_last = BUFFERS * CAPTURE_DEPTH - 1;
static uint32_t display_sample_first = 0, display_sample_last = BUFFERS * CAPTURE_DEPTH - 1;
static volatile uint16_t* capture_buffer = 0;
static uint16_t* display_buffer = 0;
static int offset = 0;
static uint dma_chan;
static volatile unsigned short stop_capture;
static uint16_t last_value, trigger_level = 24 * V5 / 50 + 1;
static int32_t display_trigger_offset = 50;         // offset from start of buffer in samples
static int32_t display_trigger_position = 100 * 10; // trigger_offset in 1uS units
static int32_t trigger_position, trigger_offset;
static uint32_t trigger_point; // index of trigger point - saved actual pointer to trigger
static unsigned char search_rising = 1, search_falling = 0, search_either = 0, first_sample, in_trigger_mode = 0;
;
static uint32_t h_res = 100; // 100uS
static uint8_t scope_pin = 0;
static uint8_t display = 0;
static uint8_t caught = 0;
static uint8_t no_switch = 0;
static uint8_t triggered = 0;
static uint8_t scope_stopped = 1;
static uint8_t scope_subsystem_stopped = 1;
volatile uint8_t scope_running = 0;
static volatile uint8_t scope_stop_waiting = 0;
static unsigned char* fb = 0;
typedef enum {
    SMODE_ONCE,
    SMODE_NORMAL,
    SMODE_AUTO
} SCOPE_MODE;
static SCOPE_MODE scope_mode = SMODE_ONCE;
typedef enum {
    TRIGGER_POS,
    TRIGGER_NEG,
    TRIGGER_NONE,
    TRIGGER_BOTH
} TRIGGER_TYPE;
static TRIGGER_TYPE trigger_type = TRIGGER_POS;
static uint16_t dy = 100;   // Y size in 10mV units
static int16_t yoffset = 0; // y offset in dy units

static uint32_t display_timebase = 500000;      // fastest possible - displayed timebase
static uint32_t display_base_timebase = 500000; // fastest possible - sample rate
static uint32_t timebase = 500000;              // fastest possible - captured timebase
static uint32_t base_timebase = 500000;         // fastest possible - sample rate
static volatile uint32_t trigger_skip;          // 100uS
static uint16_t display_zoom = 1;
static uint16_t display_samples = 1;
static uint16_t zoom = 1;    // pixels/sample at the current timebase
static uint16_t samples = 1; // samples/pixel at the current timebase
static uint16_t xoffset = 0; // x offset in dy units
static bool auto_wakeup_triggered = 0;

// b0   RRRr rGGG
// b1   gggB BBbb
unsigned short clr[] = {
    0x0000, // black
    0xffff, // white
    0x07ff, // yellow
    0x187f, // blue
    0x2086, // light green
    0x14a5, // gray
    0x1f00, // bright blue
    0x00f0, // red
    0xe007, // green
    0xa041, // pale yellow
};

typedef enum {
    BL = 0,
    WH = 1,
    Y = 2,
    B = 3,
    LG = 4,
    GY = 5,
    BB = 6,
    R = 7,
    G = 8,
    PY = 9,
} CLR;

static void scope_start(int pin);
static void scope_stop(void);
static void scope_shutdown(int now);
static void switch_buffers(void);

const char* scope_error(void) {
    return GET_T(T_MODE_ERROR_NO_EFFECT_HIZ);
}

void scope_settings(void) {
    printf("Scope ()=()");
}

void scope_help(void) {
    printf("General sub commands in all of t,x,y\r\n");
    printf("	^ up\n");
    printf("	v down\r\n");
    printf("	< left\r\n");
    printf("	> right\r\n");
    printf("	T move to trigger\r\n");
    printf("	r run again\r\n");
    printf("	o once\r\n");
    printf("	n normal\r\n");
    printf("	a auto\r\n");
    printf("	s stop\r\n");
    printf("t - trigger (movement move trigger V/time)\r\n");
    // printf("	a analog pin is the trigger pin\r\n");
    // printf("	0-7 which digital pin is the trigger pin\r\n");
    // printf("	v [0-9].[0-9] voltage level\r\n");
    printf("	+-*b  trigger on pos neg none both\r\n");
    printf("	BME move trigger point to beginning/middle/end\r\n");
    printf("\r\n");
    printf("x - timebase\r\n");
    printf("	+ faster\r\n");
    printf("	- slower\r\n");
    printf("\r\n");
    printf("y - y scale\r\n");
    printf("	+ increase scale\r\n");
    printf("	- decrease scale\r\n");
    printf("\r\n");
    printf("sr - run\r\n");
    printf("	'' - same as last - button if stopped\r\n");
    printf("	0-7 - input pin\r\n");
    printf("	o - once\r\n");
    printf("	n - normal\r\n");
    printf("	a - auto\r\n");
    printf("\r\n");
    printf("ss - stop - button if running\r\n");
}

void scope_cleanup(void) {
    if (scope_running) {
        scope_stopped = 1;
        scope_shutdown(1);
    }
    scope_subsystem_stopped = 1;
    mem_free(fb);
    fb = 0;
    capture_buffer = 0;
    display_buffer = 0;
    display = 0;
    amux_sweep();
}

static unsigned char down = 0;
void scope_periodic(void) {
    if (gpio_get(EXT1)) {
        if (!down) {
            down = 1;
            // printf("down\r\n");
        }
    } else {
        if (down) {
            down = 0;
            // printf("up\r\n");
            if (!scope_running) {
                scope_stopped = 0;
                scope_start(scope_pin);
            } else {
                scope_stopped = 1;
                scope_shutdown(1);
                display = 1;
            }
        }
    }
}

uint32_t scope_setup(void) {
    uint8_t* x;

    x = mem_alloc(MALLOC_SIZE, BP_BIG_BUFFER_SCOPE);
    if (!x) {
        printf("couldn't allocate %d bytes, scope mode broken\r\n", MALLOC_SIZE);
        return 0;
    }
    mem_free(x);
    display = 1;
    return 1;
}

uint32_t scope_setup_exc(void) {
    uint8_t* x;

    x = mem_alloc(MALLOC_SIZE, BP_BIG_BUFFER_SCOPE);
    if (!x) {
        return 0;
    }

    lcd_enable();

    fb = x;
    display_buffer = (uint16_t*)&x[VS * HS / 2];                                        // 2 byte aligned
    capture_buffer = (volatile uint16_t*)&x[VS * HS / 2 + 2 * BUFFERS * CAPTURE_DEPTH]; // 2 byte aligned
    display_sample_first = 0;
    display_sample_last = 0;
    scope_subsystem_stopped = 0;
    caught = 0;
    return 1;
}

static void dma_handler(void) {
    int last_offset = offset;

    dma_hw->ints0 = 1u << dma_chan;
    if (scope_subsystem_stopped) {
        return;
    }
    if (stop_capture == 1) {
        irq_set_enabled(DMA_IRQ_0, false);
        dma_channel_set_irq0_enabled(dma_chan, false);
        adc_run(false);
        dma_channel_abort(dma_chan);
        stop_capture = 0;
        scope_running = 0;
        scope_stop_waiting = 1;
        sample_last = offset + CAPTURE_DEPTH - 1;
        if (sample_last >= (CAPTURE_DEPTH * BUFFERS)) {
            sample_last -= (CAPTURE_DEPTH * BUFFERS);
        }
        return;
    }
    offset += CAPTURE_DEPTH;
    if (offset >= (CAPTURE_DEPTH * BUFFERS)) {
        offset = 0;
    }
    dma_channel_set_write_addr(dma_chan, &capture_buffer[offset], true);
    if (stop_capture) {
        stop_capture--;
    }
    if (!first_sample && offset == sample_first) {
        sample_first += CAPTURE_DEPTH;
        if (sample_first >= (CAPTURE_DEPTH * BUFFERS)) {
            sample_first = 0;
        }
    }
    first_sample = 0;
    if (!stop_capture) {
        if (trigger_skip) { // let capture buffer fill to at least trigger point
            if (trigger_skip == 1) {
                trigger_skip = 0;
                last_value = capture_buffer[last_offset + CAPTURE_DEPTH - 1];
            } else {
                trigger_skip--;
            }
            return;
        }
        if (search_rising) {
            unsigned short v, *tp = (unsigned short*)&capture_buffer[last_offset];
            for (int i = 0; i < CAPTURE_DEPTH; i++) {
                v = *tp++;
                if (last_value <= trigger_level && v > trigger_level) {
                    triggered = 1;
                    trigger_point = i + last_offset;
                    stop_capture = BUFFERS - 1 - (trigger_offset + CAPTURE_DEPTH - 1) / CAPTURE_DEPTH;
                    break;
                }
                last_value = v;
            }
        } else if (search_falling) {
            unsigned short v, *tp = (unsigned short*)&capture_buffer[last_offset];
            for (int i = 0; i < CAPTURE_DEPTH; i++) {
                v = *tp++;
                if (last_value >= trigger_level && v < trigger_level) {
                    triggered = 1;
                    trigger_point = i + last_offset;
                    stop_capture = BUFFERS - 1 - (trigger_offset + CAPTURE_DEPTH - 1) / CAPTURE_DEPTH;
                    break;
                }
                last_value = v;
            }
        } else if (search_either) {
            unsigned short v, *tp = (unsigned short*)&capture_buffer[last_offset];
            for (int i = 0; i < CAPTURE_DEPTH; i++) {
                v = *tp++;
                if (last_value >= trigger_level ? v < trigger_level : v > trigger_level) {
                    triggered = 1;
                    trigger_point = i + last_offset;
                    stop_capture = BUFFERS - 1 - (trigger_offset + CAPTURE_DEPTH - 1) / CAPTURE_DEPTH;
                    break;
                }
                last_value = v;
            }
        }
    }
}

static void scope_stop(void) {
    if (scope_running) {
        return;
    }
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_set_irq0_enabled(dma_chan, false);
    adc_run(false);
    dma_channel_abort(dma_chan);
    adc_fifo_drain();
    dma_channel_acknowledge_irq0(dma_chan);
    dma_hw->ints0 = 1u << dma_chan;
    dma_channel_cleanup(dma_chan);
    dma_channel_unclaim(dma_chan);
    display = 1;
    if (triggered) {
        int v = trigger_point - trigger_offset;

        if (v < 0) {
            sample_first = v + (BUFFERS * CAPTURE_DEPTH);
        } else {
            sample_first = v;
        }
    }
    amux_sweep();
    system_config.info_bar_changed = 1;
}

static void scope_shutdown(int now) {
    if (!scope_running) {
        return;
    }
    if (!stop_capture) {
        stop_capture = (now ? 1 : BUFFERS - 5);
    }
    for (int i = 0; i < 10000; i++) {
        if (!stop_capture) {
            break;
        }
        busy_wait_ms(1);
    }
    scope_stop();
    no_switch = 0;
}

static void scope_restart(int pin) {
    scope_shutdown(1);
    scope_start(pin);
}

static void scope_start(int pin) {
    if (scope_running) {
        return;
    }
    triggered = 0;
    sample_first = 0;
    sample_last = 0;
    search_falling = trigger_type == TRIGGER_NEG;
    search_either = trigger_type == TRIGGER_BOTH;
    search_rising = trigger_type == TRIGGER_POS;
    scope_stop_waiting = 0;
    first_sample = 1;
    // adc_init();
    // adc_gpio_init(CURRENT_SENSE);
    // adc_gpio_init(AMUX_OUT);
    adc_select_input(AMUX_OUT_ADC);
    amux_select_bio(pin);
    timebase = display_timebase;

    adc_fifo_setup(true,  // Write each completed conversion to the sample FIFO
                   true,  // Enable DMA data request (DREQ)
                   1,     // DREQ (and IRQ) asserted when at least 1 sample present
                   false, // We won't see the ERR bit because of 8 bit reads; disable.
                   false  // Shift each sample to 8 bits when pushing to FIFO
    );
    zoom = 1;
    switch (timebase) {
        case 5000000:
            samples = 1;
            zoom = 10;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        case 2500000:
            samples = 1;
            zoom = 5;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        case 1000000:
            samples = 1;
            zoom = 2;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        case 500000:
            samples = 1;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        case 250000:
            samples = 2;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        case 100000:
            samples = 5;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        case 50000:
            samples = 10;
            adc_set_clkdiv(0);
            base_timebase = 500000;
            break;
        default:
            samples = 10;
            adc_set_clkdiv((5 * 9600000 / 10) / timebase - 1);
            base_timebase = timebase;
            break;
    }
    trigger_offset = convert_trigger_position(display_trigger_position);
    trigger_position = display_trigger_position;
    trigger_skip = trigger_offset / CAPTURE_DEPTH + 2;

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_hw->ints0 = 1u << dma_chan;
    offset = 0;
    stop_capture = 0;
    dma_channel_configure(dma_chan,
                          &cfg,
                          &capture_buffer[0], // dst
                          &adc_hw->fifo,      // src
                          CAPTURE_DEPTH,      // transfer count
                          true                // start
    );

    // memset((void *)&capture_buffer[0], 0, sizeof(capture_buffer));
    scope_running = 1;
    busy_wait_ms(1);
    adc_run(true);
    system_config.info_bar_changed = 1;
}

static uint8_t interactive;
char last;
static char next_char(void) {
    char c;

    if (interactive) {
        for (;;) {
            if (rx_fifo_try_get(&c)) {
                if (last == 0x1b && c == 0x1b) {
                    return 0;
                }
                if (c == '\r') {
                    return 0;
                }
                last = c;
                return c;
            }
            if (gpio_get(EXT1)) {
                if (!down) {
                    down = 1;
                    continue;
                }
            } else {
                if (down) {
                    down = 0;
                    if (!scope_running) {
                        return 'r';
                    } else {
                        return 's';
                    }
                }
            }
            busy_wait_ms(10);
        }
    } else {
        if (!cmdln_try_peek(0, &c)) {
            return 0;
        }
        if (c == ';' || c == '&' || c == '|') {
            return 0;
        }
        cmdln_try_discard(1);
        return c;
    }
}

static void trigger_up(void) {
    int d = trigger_level + (dy * 5 * 8 * 100 / V5 - 1);
    if (d >= V5) {
        d = V5 - 1;
    }
    trigger_level = d;
    display = 1;
}

static void trigger_down(void) {
    int d = trigger_level - (dy * 5 * 8 * 100 / V5 - 1);
    if (d < 0) {
        d = 0;
    }
    trigger_level = d;
    display = 1;
}

static int convert_trigger_position(int pos) {
    int s = 10000000 / display_base_timebase; // samples/10uS
    // int v = pos*display_zoom/(display_samples*s);
    int v = pos / s;
    if (v < 0) {
        return 0;
    }
    if (v >= (CAPTURE_DEPTH * (BUFFERS - 1))) {
        return (CAPTURE_DEPTH * (BUFFERS - 1)) - 1;
    }
    return v;
}

static void trigger_left(void) {
    int s = 10000000 / display_base_timebase; // samples/10uS
    display_trigger_position -= 10 * s * display_samples / display_zoom;
    if (display_trigger_position < 0) {
        display_trigger_offset = 0;
        display_trigger_position = 0;
    } else {
        display_trigger_offset = convert_trigger_position(display_trigger_position);
    }
    display = 1;
}

static void trigger_right(void) {
    int s = 10000000 / display_base_timebase; // samples/10uS
    display_trigger_position += 10 * s * display_samples / display_zoom;
    display_trigger_offset = convert_trigger_position(display_trigger_position);
    display = 1;
}

void trigger_begin(void) {
    int s = 5000000 / display_base_timebase; // samples/10uS
    display_trigger_position = 100 * s * display_samples / display_zoom;
    display_trigger_offset = convert_trigger_position(display_trigger_position);
    display = 1;
}

void trigger_middle(void) {
    int s = 5000000 / display_base_timebase; // samples/10uS
    display_trigger_position = (CAPTURE_DEPTH * (BUFFERS - 1)) * s * display_samples / (display_zoom * 2);
    display_trigger_offset = convert_trigger_position(display_trigger_position);
    display = 1;
}

void trigger_end(void) {
    int s = 5000000 / display_base_timebase; // samples/10uS
    display_trigger_position =
        (CAPTURE_DEPTH * (BUFFERS - 1)) * s * display_samples / display_zoom - 50 * s * display_samples / display_zoom;
    display_trigger_offset = convert_trigger_position(display_trigger_position);
    display = 1;
}

static void scope_up(void) {
    if ((yoffset + 5) * dy < 500) {
        yoffset++;
        display = 1;
    }
}

static void scope_down(void) {
    if (yoffset > -1) {
        yoffset--;
        display = 1;
    }
}

static void scope_right(void) {
    xoffset++;
    display = 1;
}

static void scope_left(void) {
    if (xoffset) {
        xoffset--;
    }
    display = 1;
}

static void scope_to_trigger(void) {
    int toffset = display_trigger_offset * display_zoom / display_samples - ((HS / 50) * 50) / 2;
    xoffset = toffset / 50;
    if (xoffset < 0) {
        xoffset == 0;
    }
    display = 1;
}

uint32_t scope_commands(struct command_result* result) {
    char c;

    //
    //	commands:
    //
    //	x - scope X  timebase
    //	y - scope y  voltage scaling
    //  t - scope trigger +-nb up down left right ports- num
    //  sr - run [pin]
    //  ss - stop
    //
    //
    // hack to discard the command
    char args[5];
    cmdln_args_string_by_position(0, sizeof(args), args);
    if (!(args[0] == 'x' || args[0] == 'y' || args[0] == 't' || (args[0] == 's' && args[1] == 'r') ||
          (args[0] == 's' && args[1] == 's'))) {
        return 0;
    }

    if (args[0]) {
        cmdln_try_discard(1);
        if (args[1]) {
            cmdln_try_discard(1);
        }
    }

    last = 0;
    for (;;) {
        interactive = !cmdln_try_peek(0, &c);
        if (interactive) {
            break;
        }
        if (c == ' ' || c == '\t') {
            cmdln_try_discard(1);
            continue;
        }
        break;
    }

    if (strcmp(args, "t") == 0) {
        // trigger
        // a 0-7 which is the trigger pin
        // v [0-9].[0-9] voltage level
        // +-*b  trigger on pos neg none both
        // ^ voltage up 0.1
        // v voltage down 0.1
        // < move trigger towards start
        // > move trigger towards end
        // p [0-1024] - trigger position
    do_t:
        in_trigger_mode = 1;
        display = 1;
        printf("Trigger: +-*b ^v<>T BME xy rsona> ");
        // printf("Trigger: a 0-7 +-*b ^v<>T BME xy rsona> ");
        while ((c = next_char()) != 0) {
            switch (c) {
                case 'x':
                    printf("\r\n");
                    goto do_x;
                case 'y':
                    printf("\r\n");
                    goto do_y;
                case '.':
                case 's':
                    scope_stopped = 1;
                    scope_shutdown(1);
                    no_switch = 1;
                    system_config.info_bar_changed = 1;
                    ;
                    break;
                case 'r':
                    scope_stopped = 0;
                    scope_restart(scope_pin);
                    break;
                case 'o':
                    scope_stopped = 0;
                    scope_mode = SMODE_ONCE;
                    scope_restart(scope_pin);
                    break;
                case 'n':
                    scope_stopped = 0;
                    scope_mode = SMODE_NORMAL;
                    scope_restart(scope_pin);
                    break;
                case 'a':
                    scope_stopped = 0;
                    scope_mode = SMODE_AUTO;
                    scope_restart(scope_pin);
                    break;
                case 'T':
                    scope_to_trigger();
                    break;
                case 0x1b:
                    c = next_char();
                    if (c != '[') {
                        break;
                    }
                    c = next_char();
                    switch (c) {
                        case 'A':
                            trigger_up();
                            break;
                        case 'B':
                            trigger_down();
                            break;
                        case 'C':
                            trigger_right();
                            break;
                        case 'D':
                            trigger_left();
                            break;
                    }
                    break;
                case '=':
                case '+':
                    trigger_type = TRIGGER_POS;
                    display = 1;
                    break;
                case '_':
                case '-':
                    trigger_type = TRIGGER_NEG;
                    display = 1;
                    break;
                case '*':
                    trigger_type = TRIGGER_NONE;
                    display = 1;
                    break;
                case 'b':
                    trigger_type = TRIGGER_BOTH;
                    display = 1;
                    break;
                case 'v':
                case 'V':
                    trigger_down();
                    break;
                case '^':
                    trigger_up();
                    break;
                case '<':
                    trigger_left();
                    break;
                case '>':
                    trigger_right();
                    break;
                case 'B':
                    trigger_begin();
                    break;
                case 'M':
                case 'm':
                    trigger_middle();
                    break;
                case 'E':
                case 'e':
                    trigger_end();
                    break;
            }
        }
        in_trigger_mode = 0;
        return 1;
    } else if (strcmp(args, "x") == 0) {
        int z;
        // x - timebase
        // < move left
        // > move right
        // + faster
        // - slower
    do_x:
        in_trigger_mode = 0;
        display = 1;
        printf("Timebase: +- ^v<>T ty rsona> ");
        while ((c = next_char()) != 0) {
            switch (c) {
                case 't':
                    printf("\r\n");
                    goto do_t;
                case 'y':
                    printf("\r\n");
                    goto do_y;
                case '.':
                case 's':
                    scope_stopped = 1;
                    scope_shutdown(1);
                    no_switch = 1;
                    system_config.info_bar_changed = 1;
                    ;
                    break;
                case 'r':
                    scope_stopped = 0;
                    scope_restart(scope_pin);
                    break;
                case 'o':
                    scope_stopped = 0;
                    scope_mode = SMODE_ONCE;
                    scope_restart(scope_pin);
                    break;
                case 'n':
                    scope_stopped = 0;
                    scope_mode = SMODE_NORMAL;
                    scope_restart(scope_pin);
                    break;
                case 'a':
                    scope_stopped = 0;
                    scope_mode = SMODE_AUTO;
                    scope_restart(scope_pin);
                    break;
                case 'T':
                    scope_to_trigger();
                    break;
                case 0x1b:
                    c = next_char();
                    if (c != '[') {
                        break;
                    }
                    c = next_char();
                    switch (c) {
                        case 'A':
                            scope_up();
                            break;
                        case 'B':
                            scope_down();
                            break;
                        case 'C':
                            scope_right();
                            break;
                        case 'D':
                            scope_left();
                            break;
                    }
                    break;
                case '=':
                case '+':
                    if (display_timebase == 5000000) {
                        break;
                    }
                    switch (display_timebase) {
                        case 2500000:
                            display_timebase = 5000000;
                            z = 1;
                            break; // display_zoom only
                        case 1000000:
                            display_timebase = 2500000;
                            z = 0;
                            break; // display_zoom only
                        case 500000:
                            display_timebase = 1000000;
                            z = 1;
                            break; // display_zoom only
                        case 250000:
                            display_timebase = 500000;
                            z = 1;
                            break; // 1 sample
                        case 100000:
                            display_timebase = 250000;
                            z = 0;
                            break; // 2 samples
                        case 50000:
                            display_timebase = 100000;
                            z = 1;
                            break; // 5 samples
                        case 25000:
                            display_timebase = 50000;
                            z = 1;
                            break;
                        case 10000:
                            display_timebase = 25000;
                            z = 0;
                            break;
                        case 5000:
                            display_timebase = 10000;
                            z = 1;
                            break;
                        case 2500:
                            display_timebase = 5000;
                            z = 1;
                            break;
                        case 1000:
                            display_timebase = 2500;
                            z = 0;
                            break;
                        case 500:
                            display_timebase = 1000;
                            z = 1;
                            break;
                        case 250:
                            display_timebase = 500;
                            z = 1;
                            break;
                        case 100:
                            display_timebase = 250;
                            z = 0;
                            break;
                        case 50:
                            display_timebase = 100;
                            z = 1;
                            break;
                        case 25:
                            display_timebase = 50;
                            z = 1;
                            break;
                        case 10:
                            display_timebase = 25;
                            z = 0;
                            break;
                    }
                    if (z) {
                        if (display_samples != 1) {
                            display_samples /= 2;
                        } else {
                            display_zoom *= 2;
                        }
                        xoffset *= 2;
                    } else {
                        if (display_samples != 1) {
                            display_samples *= 2;
                            display_samples /= 5;
                            if (display_samples == 0) {
                                display_samples = 1;
                            }
                        } else {
                            display_zoom *= 5;
                            display_zoom /= 2;
                        }
                        xoffset *= 5;
                        xoffset /= 2;
                    }
                    display_trigger_offset = convert_trigger_position(display_trigger_position);
                    display = 1;
                    break;
                case '_':
                case '-':
                    if (display_timebase == 10) {
                        break;
                    }
                    switch (display_timebase) {
                        case 5000000:
                            display_timebase = 2500000;
                            z = 1;
                            break;
                        case 2500000:
                            display_timebase = 1000000;
                            z = 0;
                            break;
                        case 1000000:
                            display_timebase = 500000;
                            z = 1;
                            break;
                        case 500000:
                            display_timebase = 250000;
                            z = 1;
                            break;
                        case 250000:
                            display_timebase = 100000;
                            z = 0;
                            break;
                        case 100000:
                            display_timebase = 50000;
                            z = 1;
                            break;
                        case 50000:
                            display_timebase = 25000;
                            z = 1;
                            break;
                        case 25000:
                            display_timebase = 10000;
                            z = 0;
                            break;
                        case 10000:
                            display_timebase = 5000;
                            z = 1;
                            break;
                        case 5000:
                            display_timebase = 2500;
                            z = 1;
                            break;
                        case 2500:
                            display_timebase = 1000;
                            z = 0;
                            break;
                        case 1000:
                            display_timebase = 500;
                            z = 1;
                            break;
                        case 500:
                            display_timebase = 250;
                            z = 1;
                            break;
                        case 250:
                            display_timebase = 100;
                            z = 0;
                            break;
                        case 100:
                            display_timebase = 50;
                            z = 1;
                            break;
                        case 50:
                            display_timebase = 25;
                            z = 1;
                            break;
                        case 25:
                            display_timebase = 10;
                            z = 0;
                            break;
                    }
                    if (z) {
                        if (display_zoom == 1) {
                            display_samples *= 2;
                        } else {
                            display_zoom /= 2;
                        }
                        xoffset /= 2;
                    } else {
                        if (display_zoom == 1) {
                            display_samples *= 5;
                            display_samples /= 2;
                        } else {
                            display_zoom *= 2;
                            display_zoom /= 5;
                            if (display_zoom == 0) {
                                display_zoom = 1;
                            }
                        }
                        xoffset *= 2;
                        xoffset /= 5;
                    }
                    display_trigger_offset = convert_trigger_position(display_trigger_position);
                    display = 1;
                    break;
                case 'v':
                case 'V':
                    scope_down();
                    break;
                case '^':
                    scope_up();
                    break;
                case '<':
                    scope_left();
                    break;
                case '>':
                    scope_right();
                    break;
            }
        }
        return 1;
    } else if (strcmp(args, "y") == 0) {
        // y - y scale
        // + increase scale
        // - decrease scale
        // ^ move up
        // v move down
    do_y:
        in_trigger_mode = 0;
        display = 1;
        printf("Voltage scale: +- ^v<>T tx rsona> ");
        while ((c = next_char()) != 0) {
            switch (c) {
                case 't':
                    printf("\r\n");
                    goto do_t;
                case 'x':
                    printf("\r\n");
                    goto do_x;
                case '.':
                case 's':
                    scope_stopped = 1;
                    scope_shutdown(1);
                    no_switch = 1;
                    system_config.info_bar_changed = 1;
                    ;
                    break;
                case 'r':
                    scope_stopped = 0;
                    scope_restart(scope_pin);
                    break;
                case 'o':
                    scope_stopped = 0;
                    scope_mode = SMODE_ONCE;
                    scope_restart(scope_pin);
                    break;
                case 'n':
                    scope_stopped = 0;
                    scope_mode = SMODE_NORMAL;
                    scope_restart(scope_pin);
                    break;
                case 'a':
                    scope_stopped = 0;
                    scope_mode = SMODE_AUTO;
                    scope_restart(scope_pin);
                    break;
                case 'T':
                    scope_to_trigger();
                    break;
                case 0x1b:
                    c = next_char();
                    if (c != '[') {
                        break;
                    }
                    c = next_char();
                    switch (c) {
                        case 'A':
                            scope_up();
                            break;
                        case 'B':
                            scope_down();
                            break;
                        case 'C':
                            scope_right();
                            break;
                        case 'D':
                            scope_left();
                            break;
                    }
                    break;
                case '=':
                case '+':
                    if (dy > 5) {
                        switch (dy) {
                            case 100:
                                dy = 50;
                                yoffset = 2 * yoffset;
                                break;
                            case 50:
                                dy = 20;
                                yoffset = 5 * yoffset / 2;
                                break;
                            case 20:
                                dy = 10;
                                yoffset = 2 * yoffset;
                                break;
                            case 10:
                                dy = 5;
                                yoffset = 2 * yoffset;
                                break;
                        }
                        if ((yoffset + 5) * dy >= 500) {
                            yoffset = 500 / dy - 5;
                        }
                        display = 1;
                    }
                    break;
                case '_':
                case '-':
                    if (dy < 100) {
                        switch (dy) {
                            case 50:
                                dy = 100;
                                yoffset /= 2;
                                break;
                            case 20:
                                dy = 50;
                                yoffset = yoffset * 2 / 5;
                                break;
                            case 10:
                                dy = 20;
                                yoffset = yoffset / 2;
                                break;
                            case 5:
                                dy = 10;
                                yoffset = yoffset / 2;
                                break;
                        }
                        if ((yoffset + 5) * dy >= 500) {
                            yoffset = 500 / dy - 5;
                        }
                        display = 1;
                    }
                    break;
                case 'v':
                case 'V':
                    scope_down();
                    break;
                case '^':
                    scope_up();
                    break;
                case '<':
                    scope_left();
                    break;
                case '>':
                    scope_right();
                    break;
            }
        }
        return 1;
    } else if (strcmp(args, "sr") == 0) {
        // start
        // <> same as last - button if stopped
        // 0-7 - pin
        // o - once
        // n - normal
        // a - auto
        for (;;) {
            char c;
            if (!cmdln_try_peek(0, &c)) {
                break;
            }
            cmdln_try_discard(1);
            if (c == 0) {
                break;
            }
            switch (c) {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                    scope_pin = c - '0';
                    break;
                case 'o':
                case 'O':
                    scope_mode = SMODE_ONCE;
                    break;
                case 'n':
                case 'N':
                    scope_mode = SMODE_NORMAL;
                    break;
                case 'a':
                case 'A':
                    scope_mode = SMODE_AUTO;
                    break;
                case ' ':
                    break;
                default:
                    printf("invalid start mode '%c'\r\n", c);
                    return 0;
            }
        }
        scope_stopped = 0;
        scope_restart(scope_pin);
    } else if (strcmp(args, "ss") == 0) {
        // stop the engine - button if started
        scope_stopped = 1;
        scope_shutdown(1);
        no_switch = 1;
        system_config.info_bar_changed = 1;
    } else {
        return 0;
    }
}

static void scope_fb_init() {
    memset(&fb[0], (BL << 4) | BL, VS * HS / 2);
}

static void draw_h_line(int x1, int x2, int y1, CLR c) {
    unsigned char* p = &fb[y1 * (HS / 2) + (x1 >> 1)];
    if (x1 & 1) {
        *p = (*p & ~0xf0) | (c << 4);
        x1++;
        p++;
    }
    int C = (c << 4) | c;
    for (int i = x1; i < x2; i += 2) {
        *p++ = C;
    }
    if (!(x2 & 1)) {
        *p = (*p & ~0xf) | c;
    }
}
static void draw_v_line(int x1, int y1, int y2, CLR c) {
    unsigned char* p = &fb[y1 * HS / 2 + (x1 >> 1)];
    if (x1 & 1) {
        for (int i = y1; i <= y2; i++) {
            *p = (*p & ~0xf0) | (c << 4);
            p += HS / 2;
        }
    } else {
        for (int i = y1; i <= y2; i++) {
            *p = (*p & ~0xf) | c;
            p += HS / 2;
        }
    }
}

static void draw_d_line(int x1, int y1, int down, CLR c) {
    unsigned char* p = &fb[y1 * HS / 2 + (x1 >> 1)];
    if (down < 0) {
        for (int i = down; i < 0; i++) {
            if (x1 & 1) {
                *p = (*p & ~0xf0) | ((int)c << 4);
                p -= HS / 2 - 1;
            } else {
                *p = (*p & ~0xf) | ((int)c);
                p -= HS / 2;
            }
            x1++;
        }
    } else {
        for (int i = 0; i < down; i++) {
            if (x1 & 1) {
                *p = (*p & ~0xf0) | ((int)c << 4);
                p += HS / 2 + 1;
            } else {
                *p = (*p & ~0xf) | ((int)c);
                p += HS / 2;
            }
            x1++;
        }
    }
}

static void draw_grid(CLR c) {
    scope_fb_init();
    for (int i = 0; i < 5; i++) {
        draw_h_line(0, HS - 1, i * (VS / 5), c);
    }
    draw_h_line(0, HS - 1, VS - 1, c);
    for (int i = 0; i < 7; i++) {
        draw_v_line(i * 50, 0, VS - 1, c);
    }
    // draw_v_line(HS-1, 0, VS-1, c);
}

static void draw_text(int x, int y, const FONT_INFO* font, CLR c, char* cp) // stolen from lcd_write_string
{
    unsigned char* b = &fb[x >> 1];
    int yHS = HS / 2 * y;
    while (*cp) {
        uint8_t adjusted_c = (*cp) - (*font).start_char;
        for (uint16_t col = 0; col < (*font).lookup[adjusted_c].width; col++) {
            uint16_t row = 0;
            uint16_t rows = (*font).lookup[adjusted_c].height;
            uint16_t offset = (*font).lookup[adjusted_c].offset;
            int off = yHS;
            for (uint16_t page = 0; page < (*font).height_bytes; page++) {
                uint8_t bitmap_char = (*font).bitmaps[offset + (col * (*font).height_bytes) + page];
                for (uint8_t i = 0; i < 8; i++) {
                    if (off >= 0 && bitmap_char & (0x80 >> i)) {
                        unsigned char* p = &b[off];
                        if (x & 1) {
                            *p = (*p & ~0xf0) | (c << 4);
                        } else {
                            *p = (*p & ~0xf) | (c);
                        }
                    }
                    off -= HS / 2;
                    row++;
                    if (row == rows) {
                        break;
                    }
                }
            }
            if (x & 1) {
                b++;
            }
            x++;
        }
        cp++;
    }
}

static void draw_trace(CLR c) {
    unsigned char* p = &fb[0];
    uint16_t* b = (uint16_t*)&display_buffer[0]; // only called when not volatile
    int16_t v1[HS];
    int16_t v2[HS];
    int x;
    int y1;
    int y2;
    // dy is the number of 10 mv per
    // yoffset is the offest from 0 in dy units
    int df = yoffset * (VS / 5);
    uint32_t offset = display_sample_first + xoffset * 50 * display_samples / display_zoom;
    if (offset >= (BUFFERS * CAPTURE_DEPTH)) {
        offset -= BUFFERS * CAPTURE_DEPTH;
    }
    b += offset;
    int xlast = HS;
    if (display_sample_first <= display_sample_last) {
        if (offset > display_sample_last || offset < display_sample_first) {
            return;
        }
    } else {
        if (offset > display_sample_last && offset < display_sample_first) {
            return;
        }
    }
    for (x = 0; x < HS;) {
        for (int i = 0; i < display_samples; i++) {
            uint32_t y = *b++;

            int d = (y * VS * 100 / V5 / dy) - df;
            if (d < 0) {
                d = -1;
            } else {
                if (d >= VS) {
                    d = VS;
                }
            }
            if (i == 0) {
                v1[x] = d;
                v2[x] = d;
            } else {
                if (d < v1[x]) {
                    v1[x] = d;
                }
                if (d > v2[x]) {
                    v2[x] = d;
                }
            }
            if (offset == display_sample_last) {
                x += display_zoom;
                goto xdone;
            }
            offset++;
            if (offset >= (BUFFERS * CAPTURE_DEPTH)) {
                offset = 0;
                b = (uint16_t*)&display_buffer[0];
            }
        }
        x += display_zoom;
    }
xdone:
    xlast = x;
    y1 = v1[0];
    y2 = v2[0];
    if (v1[display_zoom] > y2) {
        y2 = (y2 + v1[display_zoom]) / 2;
    } else if (v2[display_zoom] < y1) {
        y1 = (y1 + v2[display_zoom]) / 2;
    }
    if ((y1 >= 0 && y1 < VS) || (y2 >= 0 && y2 < VS)) {
        if (y1 >= VS) {
            y1 = VS - 1;
        }
        if (y2 >= VS) {
            y2 = VS - 1;
        }
        if (y1 < 0) {
            y1 = 0;
        }
        if (y2 < 0) {
            y2 = 0;
        }
        if (y1 > y2) {
            int t = y1;
            y1 = y2;
            y2 = t;
        }
        if (display_zoom == 1) {
            for (int y = y1; y <= y2; y++) {
                p[(HS / 2) * y + 0] = (p[(HS / 2) * y + 0] & ~0xf) | c;
            }
        } else {
            if (y1 > v1[display_zoom]) {
                int yf = y2;
                for (x = 0; x < display_zoom; x++) {
                    int yl = y2 - (x + 1) * (y2 - y1) / display_zoom;
                    for (int y = yf; y >= yl; y--) {
                        if (x & 1) {
                            p[(HS / 2) * y + (x >> 1)] = (p[(HS / 2) * y + (x >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 2) * y + (x >> 1)] = (p[(HS / 2) * y + (x >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            } else {
                int yf = y1;
                for (x = 0; x < display_zoom; x++) {
                    int yl = y1 + (x + 1) * (y2 - y1) / display_zoom;
                    for (int y = yf; y <= yl; y++) {
                        if (x & 1) {
                            p[(HS / 2) * y + (x >> 1)] = (p[(HS / 2) * y + (x >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 2) * y + (x >> 1)] = (p[(HS / 2) * y + (x >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            }
        }
    }
    for (x = display_zoom; x < (xlast - display_zoom); x += display_zoom) {
        y1 = v1[x];
        y2 = v2[x];

        if (v1[x + display_zoom] > y2) {
            if (v1[x - display_zoom] > v1[x + display_zoom]) {
                y2 = (y2 + v1[x - display_zoom]);
            } else {
                y2 = (y2 + v1[x + display_zoom]);
            }
        } else if (v1[x - display_zoom] > y2) {
            y2 = (y2 + v1[x - display_zoom]);
        } else {
            y2 = y2 << 1;
        }

        if (v2[x + display_zoom] < y1) {
            if (v2[x - display_zoom] < v2[x + display_zoom]) {
                y1 = (y1 + v2[x - display_zoom]);
            } else {
                y1 = (y1 + v2[x + display_zoom]);
            }
        } else if (v2[x - display_zoom] < y1) {
            y1 = (y1 + v2[x - display_zoom]);
        } else {
            y1 = y1 << 1;
        }

        if (y1 >= (2 * VS)) {
            if (y2 >= (2 * VS)) {
                continue;
            }
            y1 = 2 * (VS - 1);
        } else if (y2 >= (2 * VS)) {
            y2 = 2 * (VS - 1);
        }

        if (y1 < 0) {
            if (y2 < 0) {
                continue;
            }
            y1 = 0;
        } else if (y2 < 0) {
            y2 = 0;
        }
        if (display_zoom == 1) {
            for (int y = y1 & ~1; y <= y2; y += 2) {
                if (x & 1) {
                    p[(HS / 4) * y + (x >> 1)] = (p[(HS / 4) * y + (x >> 1)] & ~0xf0) | (c << 4);
                } else {
                    p[(HS / 4) * y + (x >> 1)] = (p[(HS / 4) * y + (x >> 1)] & ~0xf) | (c);
                }
            }
        } else {
            int v1a = (v1[x - display_zoom] << 1);
            int v1v = (v1[x] << 1);
            int v1b = (v1[x + display_zoom] << 1);
            if (v1a < v1v && v1b < v1v) {
                y1 = (v1a + v1v) >> 1;
                y2 = (v1b + v1v) >> 1;
                int yf = y1 & ~1;
                int z2 = display_zoom / 2;
                int z4 = display_zoom / 4;
                int zr2 = display_zoom - z2;
                for (int xx = 0; xx < z2; xx++) {
                    int yl = (y1 + ((xx + 1) * (v1v - y1) + z4) / z2) & ~1;
                    for (int y = yf; y <= yl; y += 2) {
                        if ((x + xx) & 1) {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
                yf = v1v & ~1;
                for (int xx = 0; xx < zr2; xx++) {
                    int yl = (v1v - ((xx + 1) * (v1v - y2) - z4) / zr2) & ~1;
                    for (int y = yf; y >= yl; y -= 2) {
                        if ((x + z2 + xx) & 1) {
                            p[(HS / 4) * y + ((x + z2 + xx) >> 1)] =
                                (p[(HS / 4) * y + ((x + z2 + xx) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 4) * y + ((x + z2 + xx) >> 1)] =
                                (p[(HS / 4) * y + ((x + z2 + xx) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            } else if (v1a > v1v && v1b > v1v) {
                y1 = (v1a + v1v) >> 1;
                y2 = (v1b + v1v) >> 1;
                int yf = y1 & ~1;
                int z2 = display_zoom / 2;
                int z4 = display_zoom / 4;
                int zr2 = display_zoom - z2;
                for (int xx = 0; xx < z2; xx++) {
                    int yl = (y1 - ((xx + 1) * (y1 - v1v) - z4) / z2) & ~1;
                    for (int y = yf; y >= yl; y -= 2) {
                        if ((x + xx) & 1) {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
                yf = v1v & ~1;
                for (int xx = 0; xx < zr2; xx++) {
                    int yl = (v1v + ((xx + 1) * (y2 - v1v) + z4) / zr2) & ~1;
                    for (int y = yf; y <= yl; y += 2) {
                        if ((x + z2 + xx) & 1) {
                            p[(HS / 4) * y + ((x + z2 + xx) >> 1)] =
                                (p[(HS / 4) * y + ((x + z2 + xx) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 4) * y + ((x + z2 + xx) >> 1)] =
                                (p[(HS / 4) * y + ((x + z2 + xx) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            } else if (v1b < v1v || v1a > v1v) {
                int yf = y2 & ~1;
                for (int xx = 0; xx < display_zoom; xx++) {
                    int yl = (y2 - (xx + 1) * (y2 - y1) / display_zoom) & ~1;
                    for (int y = yf; y >= yl; y -= 2) {
                        if ((x + xx) & 1) {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            } else {
                int yf = y1 & ~1;
                for (int xx = 0; xx < display_zoom; xx++) {
                    int yl = (y1 + (xx + 1) * (y2 - y1) / display_zoom) & ~1;
                    for (int y = yf; y <= yl; y += 2) {
                        if ((x + xx) & 1) {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 4) * y + ((x + xx) >> 1)] = (p[(HS / 4) * y + ((x + xx) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            }
        }
    }
    y1 = v1[xlast - display_zoom];
    y2 = v2[xlast - display_zoom];
    if (v1[xlast - 2 * display_zoom] > y2) {
        y2 = (y2 + v1[xlast - 2 * display_zoom]) / 2;
    }
    if (v2[xlast - 2 * display_zoom] < y1) {
        y1 = (y1 + v2[xlast - 2 * display_zoom]) / 2;
    }
    if ((y1 >= 0 && y1 < VS) || (y2 >= 0 && y2 < VS)) {
        if (y1 >= VS) {
            y1 = VS - 1;
        }
        if (y2 >= VS) {
            y2 = VS - 1;
        }
        if (y1 < 0) {
            y1 = 0;
        }
        if (y2 < 0) {
            y2 = 0;
        }
        if (y1 > y2) {
            int t = y1;
            y1 = y2;
            y2 = t;
        }
        if (display_zoom == 1) {
            for (int y = y1; y <= y2; y++) {
                if ((xlast - 1) & 1) {
                    p[(HS / 2) * y + ((xlast - 1) >> 1)] = (p[(HS / 2) * y + ((xlast - 1) >> 1)] & ~0xf0) | (c << 4);
                } else {
                    p[(HS / 2) * y + ((xlast - 1) >> 1)] = (p[(HS / 2) * y + ((xlast - 1) >> 1)] & ~0xf) | (c);
                }
            }
        } else {
            if (v1[xlast - display_zoom] < v1[xlast - 2 * display_zoom]) {
                int yf = y2;
                for (x = 0; x < display_zoom; x++) {
                    int yl = y2 - (x + 1) * (y2 - y1) / display_zoom;
                    for (int y = yf; y >= yl; y--) {
                        if ((x + xlast - display_zoom) & 1) {
                            p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] =
                                (p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] =
                                (p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            } else {
                int yf = y1;
                for (x = 0; x < display_zoom; x++) {
                    int yl = y1 + (x + 1) * (y2 - y1) / display_zoom;
                    for (int y = yf; y <= yl; y++) {
                        if ((x + xlast - display_zoom) & 1) {
                            p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] =
                                (p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] & ~0xf0) | (c << 4);
                        } else {
                            p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] =
                                (p[(HS / 2) * y + ((x + xlast - display_zoom) >> 1)] & ~0xf) | (c);
                        }
                    }
                    yf = yl;
                }
            }
        }
    }
}

static int xnum(char* cp, int v, int first) {
    if (v == 0) {
        if (first) {
            *cp = '0';
            return 1;
        } else {
            return 0;
        }
    }
    int i = xnum(cp, v / 10, 0);
    cp += i;
    v = v % 10;
    *cp = v + '0';
    return i + 1;
}

static void draw_triggers(int minimal) {
    int df = yoffset * (VS / 5);
    int d = (trigger_level * VS * 100 / V5 / dy) - df;
    if (!minimal) {
        if (d >= 0 && d < VS) {
            draw_h_line(0, HS - 1, d, R);
        } else if (d < 0) {
            draw_v_line(HS / 2, 0, 11, R);
            draw_d_line(HS / 2 - 5, 5, -5, R);
            draw_d_line(HS / 2, 0, 5, R);
        } else {
            draw_v_line(HS / 2, VS - 12, VS - 1, R);
            draw_d_line(HS / 2 - 5, VS - 6, 5, R);
            draw_d_line(HS / 2, VS - 1, -5, R);
        }
    }

    int toffset = display_trigger_offset * display_zoom / display_samples - (xoffset * 50);
    if (minimal) {
        if (toffset >= 0 && toffset < HS) {
            draw_v_line(toffset, VS - 15, VS - 4, R);
            if (toffset >= 6) {
                draw_d_line(toffset - 6, VS - 9, -6, R);
            }
            if (toffset <= (HS - 7)) {
                draw_d_line(toffset, VS - 15, 6, R);
            }
        }
        // draw_text(toffset < 6 ? 6 : toffset > (HS-6) ? HS-6 : toffset-6, VS-5, &hunter_12ptFontInfo, R, "T");
    } else {
        if (toffset < 0) {
            draw_h_line(0, 11, VS / 2, R);
            draw_d_line(0, VS / 2, -5, R);
            draw_d_line(0, VS / 2, 5, R);
        } else if (toffset >= HS) {
            draw_h_line(HS - 12, HS - 1, VS / 2, R);
            draw_d_line(HS - 6, VS / 2 + 5, -5, R);
            draw_d_line(HS - 6, VS / 2 - 5, 5, R);
        } else {
            draw_v_line(toffset, 0, VS - 1, R);
        }
        char b[30];
        char* cp = &b[0];

        switch (trigger_type) {
            case TRIGGER_POS:
                *cp++ = '+';
                break;
            case TRIGGER_NEG:
                *cp++ = '-';
                break;
            case TRIGGER_BOTH:
                *cp++ = '+';
                *cp++ = '-';
                break;
            case TRIGGER_NONE:
                break;
        }
        int d = 500 * trigger_level / V5;
        int v;
        v = d / 100;
        cp += xnum(cp, v, 1);
        v = d % 100;
        if (v) {
            *cp++ = '.';
            *cp++ = '0' + v / 10;
            v = v % 10;
            if (v) {
                *cp++ = '0' + v;
            }
        }
        *cp++ = 'V';
        *cp++ = ' ';
        int t = display_trigger_position / 10;
        if (t < 1000) {
            cp += xnum(cp, t, 1);
            strcpy(cp, "uS");
        } else {
            char* cp3;
            int v2;
            int v = t / 1000;
            if (v > 1000) {
                cp3 = "S";
                v2 = v % 1000;
                v = v / 1000;
            } else {
                cp3 = "mS";
                v2 = t % 1000;
            }
            cp += xnum(cp, v, 1);
            if (v2 != 0 && v < 100) {
                *cp++ = '.';
                *cp++ = '0' + v2 / 100;
                v2 = v2 % 100;
                if (v2 != 0 && v < 10) {
                    *cp++ = '0' + v2 / 10;
                }
            }
            strcpy(cp, cp3);
        }
        draw_text(HS - 14 - (strlen(b) * 12), VS - 18, &hunter_12ptFontInfo, R, &b[0]);
    }
}

static void draw_scope() {
    char b[40];
    char *s1, *s2;
    int unit;
    draw_grid(PY);

    switch (dy) {
        case 100:
            s1 = "1Vx";
            break;
        case 50:
            s1 = "0.5Vx";
            break;
        case 20:
            s1 = "0.2Vx";
            break;
        case 10:
            s1 = "0.1Vx";
            break;
        case 5:
            s1 = "0.05Vx";
            break;
    }
    switch (display_timebase) {
        case 5000000:
            s2 = "10uS";
            unit = 1;
            break;
        case 2500000:
            s2 = "20uS";
            unit = 2;
            break;
        case 1000000:
            s2 = "50uS";
            unit = 5;
            break;
        case 500000:
            s2 = "100uS";
            unit = 10;
            break;
        case 250000:
            s2 = "200uS";
            unit = 20;
            break;
        case 100000:
            s2 = "500uS";
            unit = 50;
            break;
        case 50000:
            s2 = "1mS";
            unit = 100;
            break;
        case 25000:
            s2 = "2mS";
            unit = 200;
            break;
        case 10000:
            s2 = "5mS";
            unit = 500;
            break;
        case 5000:
            s2 = "10mS";
            unit = 1000;
            break;
        case 2500:
            s2 = "20mS";
            unit = 2000;
            break;
        case 1000:
            s2 = "50mS";
            unit = 5000;
            break;
        case 500:
            s2 = "100mS";
            unit = 10000;
            break;
        case 250:
            s2 = "200mS";
            unit = 20000;
            break;
        case 100:
            s2 = "500mS";
            unit = 50000;
            break;
        case 50:
            s2 = "1S";
            unit = 100000;
            break;
        case 25:
            s2 = "2S";
            unit = 200000;
            break;
        case 10:
            s2 = "5S";
            unit = 500000;
            break;
    }
    strcpy(b, s1);
    strcat(b, s2);
    draw_text(HS - 12 - (strlen(b) * 12), VS - 5, &hunter_12ptFontInfo, GY, &b[0]);
    int v = (yoffset < 0) ? 0 : dy * yoffset;
    b[0] = v / 100 + '0';
    int off = 1;
    v = v % 100;
    if (v) {
        b[off++] = '.';
        b[off++] = v / 10 + '0';
        v = v % 10;
        if (v) {
            b[off++] = v + '0';
        }
    }
    b[off++] = 'V';
    b[off] = 0;
    draw_text(5, 15 + ((yoffset < 0) ? -yoffset * (VS / 5) : 0), &hunter_12ptFontInfo, GY, &b[0]);
    unit = unit * xoffset;
    if (unit >= 100000) {
        int v = unit / 100000;
        unsigned char* cp = &b[0];
        unit = unit % 100000;
        cp += xnum(cp, v, 1);
        if (unit) {
            *cp++ = '.';
            *cp++ = unit / 10000 + '0';
            unit = unit % 10000;
            if (unit) {
                *cp++ = unit / 1000 + '0';
            }
        }
        *cp++ = 'S';
        *cp = 0;
    } else if (unit >= 100) {
        int v = unit / 100;
        unsigned char* cp = &b[0];
        unit = unit % 100;
        cp += xnum(cp, v, 1);
        if (unit) {
            *cp++ = '.';
            *cp++ = unit / 10 + '0';
            unit = unit % 10;
            if (unit) {
                *cp++ = unit + '0';
            }
        }
        *cp++ = 'm';
        *cp++ = 'S';
        *cp = 0;
    } else if (unit != 0) {
        unsigned char* cp = &b[0];
        if (unit >= 10) {
            *cp++ = unit / 10 + '0';
            unit = unit % 10;
        }
        *cp++ = unit + '0';
        *cp++ = '0';
        *cp++ = 'u';
        *cp++ = 'S';
        *cp = 0;
    } else { // 0;
        unsigned char* cp = &b[0];
        *cp++ = '0';
        if (display_timebase >= 1000000) {
            *cp++ = 'u';
        } else if (display_timebase >= 1000) {
            *cp++ = 'm';
        }
        *cp++ = 'S';
        *cp = 0;
    }
    draw_text(5, VS - 5, &hunter_12ptFontInfo, GY, &b[0]);
    strcpy(b, "Pin");
    b[3] = '0' + scope_pin;
    b[4] = 0;
    draw_text(308 - (strlen(b) * 12), 15, &hunter_12ptFontInfo, GY, &b[0]);
    if (in_trigger_mode) {
        draw_triggers(0);
    } else {
        draw_triggers(1);
    }
    if (caught) {
        draw_trace(B);
    }
}

static void scope_write() {
    uint16_t x, y;

    lcd_set_bounding_box(0, VS, 0, HS);

    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 1);
    gpio_put(DISPLAY_CS, 0);

    for (uint32_t b = 0; b < (HS * VS / 2); b += 1) {
        unsigned char x = fb[b];
        spi_write_blocking(BP_SPI_PORT, (unsigned char*)&clr[x & 0xf], 2);
        spi_write_blocking(BP_SPI_PORT, (unsigned char*)&clr[(x >> 4) & 0xf], 2);
    }

    gpio_put(DISPLAY_CS, 1);
    spi_busy_wait(false);
}

static int64_t auto_wakeup(alarm_id_t id, void* user_data) {
    auto_wakeup_triggered = 1;
    return 0;
}

static void switch_buffers(void) {
    uint16_t* x;
    if (no_switch) {
        no_switch = 0;
    } else {
        x = display_buffer;
        display_buffer = (uint16_t*)capture_buffer;
        capture_buffer = x;
        display_sample_first = sample_first;
        display_sample_last = sample_last;
        display_zoom = zoom;
        display_samples = samples;
        int t = timebase;
        int z;
        if (t < display_timebase) { // convert samples/zoom to currently displayed resolution
            while (t != display_timebase) {
                if (t == 5000000) {
                    break;
                }
                switch (t) {
                    case 2500000:
                        t = 5000000;
                        z = 1;
                        break; // display_zoom only
                    case 1000000:
                        t = 2500000;
                        z = 0;
                        break; // display_zoom only
                    case 500000:
                        t = 1000000;
                        z = 1;
                        break; // display_zoom only
                    case 250000:
                        t = 500000;
                        z = 1;
                        break; // 1 sample
                    case 100000:
                        t = 250000;
                        z = 0;
                        break; // 2 samples
                    case 50000:
                        t = 100000;
                        z = 1;
                        break; // 5 samples
                    case 25000:
                        t = 50000;
                        z = 1;
                        break;
                    case 10000:
                        t = 25000;
                        z = 0;
                        break;
                    case 5000:
                        t = 10000;
                        z = 1;
                        break;
                    case 2500:
                        t = 5000;
                        z = 1;
                        break;
                    case 1000:
                        t = 2500;
                        z = 0;
                        break;
                    case 500:
                        t = 1000;
                        z = 1;
                        break;
                    case 250:
                        t = 500;
                        z = 1;
                        break;
                    case 100:
                        t = 250;
                        z = 0;
                        break;
                    case 50:
                        t = 100;
                        z = 1;
                        break;
                    case 25:
                        t = 50;
                        z = 1;
                        break;
                    case 10:
                        t = 25;
                        z = 0;
                        break;
                }
                if (z) {
                    if (display_samples != 1) {
                        display_samples /= 2;
                    } else {
                        display_zoom *= 2;
                    }
                } else {
                    if (display_samples != 1) {
                        display_samples *= 2;
                        display_samples /= 5;
                        if (display_samples == 0) {
                            display_samples = 1;
                        }
                    } else {
                        display_zoom *= 5;
                        display_zoom /= 2;
                    }
                }
            }
        } else if (t > display_timebase) {
            while (t != display_timebase) {
                if (t == 10) {
                    break;
                }
                switch (t) {
                    case 5000000:
                        t = 2500000;
                        z = 1;
                        break;
                    case 2500000:
                        t = 1000000;
                        z = 0;
                        break;
                    case 1000000:
                        t = 500000;
                        z = 1;
                        break;
                    case 500000:
                        t = 250000;
                        z = 1;
                        break;
                    case 250000:
                        t = 100000;
                        z = 0;
                        break;
                    case 100000:
                        t = 50000;
                        z = 1;
                        break;
                    case 50000:
                        t = 25000;
                        z = 1;
                        break;
                    case 25000:
                        t = 10000;
                        z = 0;
                        break;
                    case 10000:
                        t = 5000;
                        z = 1;
                        break;
                    case 5000:
                        t = 2500;
                        z = 1;
                        break;
                    case 2500:
                        t = 1000;
                        z = 0;
                        break;
                    case 1000:
                        t = 500;
                        z = 1;
                        break;
                    case 500:
                        t = 250;
                        z = 1;
                        break;
                    case 250:
                        t = 100;
                        z = 0;
                        break;
                    case 100:
                        t = 50;
                        z = 1;
                        break;
                    case 50:
                        t = 25;
                        z = 1;
                        break;
                    case 25:
                        t = 10;
                        z = 0;
                        break;
                }
                if (z) {
                    if (display_zoom == 1) {
                        display_samples *= 2;
                    } else {
                        display_zoom /= 2;
                    }
                } else {
                    if (display_zoom == 1) {
                        display_samples *= 5;
                        display_samples /= 2;
                    } else {
                        display_zoom *= 2;
                        display_zoom /= 5;
                        if (display_zoom == 0) {
                            display_zoom = 1;
                        }
                    }
                }
            }
        }
        // display_timebase = timebase;
        display_base_timebase = base_timebase;
        if (!in_trigger_mode) {
            display_trigger_offset = trigger_offset;
            display_trigger_position = trigger_position;
        }
        caught = 1;
    }
}

void scope_lcd_update(uint32_t flags) {
    if (scope_subsystem_stopped) {
        return;
    }
    if (!scope_running) {
        if (scope_stop_waiting) {
            scope_stop_waiting = 0;
            scope_stop();
            switch_buffers();
            display = 1;
        }
    } else {
        if (scope_mode == SMODE_AUTO && auto_wakeup_triggered) {
            scope_shutdown(1);
            switch_buffers();
            display = 1;
        }
    }
    if (!display) {
        return;
    }
    display = 0;
    draw_scope();
    // spi_set_baudrate(BP_SPI_PORT, 62500*1000);
    scope_write();
    spi_set_baudrate(BP_SPI_PORT, 32000 * 1000);
    if (!scope_running && !scope_stopped && (scope_mode == SMODE_AUTO || scope_mode == SMODE_NORMAL)) {
        auto_wakeup_triggered = 0;
        scope_start(scope_pin);
        if (scope_mode == SMODE_AUTO) {
            (void)add_alarm_in_ms(1000, auto_wakeup, NULL, false);
        }
    }
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
