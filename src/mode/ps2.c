#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "ui/ui_help.h"
#include "pirate/hwspi.h"
#include "usb_rx.h"
#include "binmode/binio.h"
#include "pirate/mem.h"

/*
MIT License - okhi - Open Keylogger Hardware Implant
---------------------------------------------------------------------------
Copyright (c) [2024] by David Reguera Garcia aka Dreg
https://github.com/therealdreg/pico-ps2-sniffer
https://www.rootkit.es
X @therealdreg
dreg@rootkit.es
---------------------------------------------------------------------------
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
---------------------------------------------------------------------------
WARNING: BULLSHIT CODE X-)
---------------------------------------------------------------------------
*/


#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico_ps2_sniffer.pio.h"
#include "tusb.h"

#define BP() __asm("bkpt #1"); // breakpoint via software macro

#define FVER 1

#define RING_BUFF_MAX_ENTRIES 800

typedef struct
{
    const unsigned char *bytes;
    const size_t num_bytes;
    const char *name;
    const char ascii;
} key_table_t;

typedef struct
{
    uint8_t byte;
    uint32_t timestamp;
    bool device_to_host;
} ps2_packet_t;

typedef struct
{
    unsigned char buffer[10];
    size_t length;
} ps2_parser_state_t;

volatile static ps2_parser_state_t parser_state = {{0}, 0};

volatile static unsigned int write_index = 0;
//volatile static unsigned char ringbuff[RING_BUFF_MAX_ENTRIES][32];

static volatile uint8_t (*ringbuff)[32] = NULL;

volatile static uint kbd_h2d_sm;
volatile static uint offset_kbd_h2d;
volatile static uint kbd_sm;
volatile static uint offset_kbd;
volatile static int inhnr;
volatile static bool last_state_idle;
volatile static int inidle;
volatile static int inidletoggle;
volatile static bool inh_fired;
volatile static bool glitch_detector_init = false;
volatile static uint kbd_glt_sm = 0;

#define BIO_DAT_GPIO BIO4         // BP BIO PS/2 data
#define BIO_CLK_GPIO BIO5         // BP BIO PS/2 clock
#define BIO_AUX_H2D_JMP_GPIO BIO6 // BP BIO PIO JMP HELPER PIN FOR HOST TO DEVICE PIO (must be a free GPIO pin)
#define BIO_AUX_D2H_JMP_GPIO BIO7 // BP BIO PIO JMP HELPER PIN FOR DEVICE TO HOST PIO (must be a free GPIO pin)


#define DAT_GPIO (BIO_DAT_GPIO + 8)                   // RP PIN PS/2 data
#define CLK_GPIO (BIO_CLK_GPIO + 8)                   // RP PIN PS/2 clock
#define AUX_H2D_JMP_GPIO (BIO_AUX_H2D_JMP_GPIO + 8)   // RP PIN PIO JMP HELPER PIN FOR HOST TO DEVICE PIO (must be a free GPIO pin)
#define AUX_D2H_JMP_GPIO  (BIO_AUX_D2H_JMP_GPIO + 8)  // RP PIN PIO JMP HELPER PIN FOR DEVICE TO HOST PIO (must be a free GPIO pin)



// Keyboard Scan Codes: Set 2

static const unsigned char MAKE_A[] = {0x1C};
static const unsigned char BREAK_A[] = {0xF0, 0x1C};

static const unsigned char MAKE_B[] = {0x32};
static const unsigned char BREAK_B[] = {0xF0, 0x32};

static const unsigned char MAKE_C[] = {0x21};
static const unsigned char BREAK_C[] = {0xF0, 0x21};

static const unsigned char MAKE_D[] = {0x23};
static const unsigned char BREAK_D[] = {0xF0, 0x23};

static const unsigned char MAKE_E[] = {0x24};
static const unsigned char BREAK_E[] = {0xF0, 0x24};

static const unsigned char MAKE_F[] = {0x2B};
static const unsigned char BREAK_F[] = {0xF0, 0x2B};

static const unsigned char MAKE_G[] = {0x34};
static const unsigned char BREAK_G[] = {0xF0, 0x34};

static const unsigned char MAKE_H[] = {0x33};
static const unsigned char BREAK_H[] = {0xF0, 0x33};

static const unsigned char MAKE_I[] = {0x43};
static const unsigned char BREAK_I[] = {0xF0, 0x43};

static const unsigned char MAKE_J[] = {0x3B};
static const unsigned char BREAK_J[] = {0xF0, 0x3B};

static const unsigned char MAKE_K[] = {0x42};
static const unsigned char BREAK_K[] = {0xF0, 0x42};

static const unsigned char MAKE_L[] = {0x4B};
static const unsigned char BREAK_L[] = {0xF0, 0x4B};

static const unsigned char MAKE_M[] = {0x3A};
static const unsigned char BREAK_M[] = {0xF0, 0x3A};

static const unsigned char MAKE_N[] = {0x31};
static const unsigned char BREAK_N[] = {0xF0, 0x31};

static const unsigned char MAKE_O[] = {0x44};
static const unsigned char BREAK_O[] = {0xF0, 0x44};

static const unsigned char MAKE_P[] = {0x4D};
static const unsigned char BREAK_P[] = {0xF0, 0x4D};

static const unsigned char MAKE_Q[] = {0x15};
static const unsigned char BREAK_Q[] = {0xF0, 0x15};

static const unsigned char MAKE_R[] = {0x2D};
static const unsigned char BREAK_R[] = {0xF0, 0x2D};

static const unsigned char MAKE_S[] = {0x1B};
static const unsigned char BREAK_S[] = {0xF0, 0x1B};

static const unsigned char MAKE_T[] = {0x2C};
static const unsigned char BREAK_T[] = {0xF0, 0x2C};

static const unsigned char MAKE_U[] = {0x3C};
static const unsigned char BREAK_U[] = {0xF0, 0x3C};

static const unsigned char MAKE_V[] = {0x2A};
static const unsigned char BREAK_V[] = {0xF0, 0x2A};

static const unsigned char MAKE_W[] = {0x1D};
static const unsigned char BREAK_W[] = {0xF0, 0x1D};

static const unsigned char MAKE_X[] = {0x22};
static const unsigned char BREAK_X[] = {0xF0, 0x22};

static const unsigned char MAKE_Y[] = {0x35};
static const unsigned char BREAK_Y[] = {0xF0, 0x35};

static const unsigned char MAKE_Z[] = {0x1A};
static const unsigned char BREAK_Z[] = {0xF0, 0x1A};

static const unsigned char MAKE_0[] = {0x45};
static const unsigned char BREAK_0[] = {0xF0, 0x45};

static const unsigned char MAKE_1[] = {0x16};
static const unsigned char BREAK_1[] = {0xF0, 0x16};

static const unsigned char MAKE_2[] = {0x1E};
static const unsigned char BREAK_2[] = {0xF0, 0x1E};

static const unsigned char MAKE_3[] = {0x26};
static const unsigned char BREAK_3[] = {0xF0, 0x26};

static const unsigned char MAKE_4[] = {0x25};
static const unsigned char BREAK_4[] = {0xF0, 0x25};

static const unsigned char MAKE_5[] = {0x2E};
static const unsigned char BREAK_5[] = {0xF0, 0x2E};

static const unsigned char MAKE_6[] = {0x36};
static const unsigned char BREAK_6[] = {0xF0, 0x36};

static const unsigned char MAKE_7[] = {0x3D};
static const unsigned char BREAK_7[] = {0xF0, 0x3D};

static const unsigned char MAKE_8[] = {0x3E};
static const unsigned char BREAK_8[] = {0xF0, 0x3E};

static const unsigned char MAKE_9[] = {0x46};
static const unsigned char BREAK_9[] = {0xF0, 0x46};

static const unsigned char MAKE_BACKTICK[] = {0x0E};
static const unsigned char BREAK_BACKTICK[] = {0xF0, 0x0E};

static const unsigned char MAKE_DASH[] = {0x4E};
static const unsigned char BREAK_DASH[] = {0xF0, 0x4E};

static const unsigned char MAKE_EQUAL[] = {0x55};
static const unsigned char BREAK_EQUAL[] = {0xF0, 0x55};

static const unsigned char MAKE_BACKSLASH[] = {0x5D};
static const unsigned char BREAK_BACKSLASH[] = {0xF0, 0x5D};

static const unsigned char MAKE_BACKSPACE[] = {0x66};
static const unsigned char BREAK_BACKSPACE[] = {0xF0, 0x66};

static const unsigned char MAKE_SPACE[] = {0x29};
static const unsigned char BREAK_SPACE[] = {0xF0, 0x29};

static const unsigned char MAKE_TAB[] = {0x0D};
static const unsigned char BREAK_TAB[] = {0xF0, 0x0D};

static const unsigned char MAKE_CAPSLOCK[] = {0x58};
static const unsigned char BREAK_CAPSLOCK[] = {0xF0, 0x58};

static const unsigned char MAKE_SHIFT_LEFT[] = {0x12};
static const unsigned char BREAK_SHIFT_LEFT[] = {0xF0, 0x12};

static const unsigned char MAKE_CTRL_LEFT[] = {0x14};
static const unsigned char BREAK_CTRL_LEFT[] = {0xF0, 0x14};

static const unsigned char MAKE_GUI_LEFT[] = {0xE0, 0x1F};
static const unsigned char BREAK_GUI_LEFT[] = {0xE0, 0xF0, 0x1F};

static const unsigned char MAKE_ALT_LEFT[] = {0x11};
static const unsigned char BREAK_ALT_LEFT[] = {0xF0, 0x11};

static const unsigned char MAKE_SHIFT_RIGHT[] = {0x59};
static const unsigned char BREAK_SHIFT_RIGHT[] = {0xF0, 0x59};

static const unsigned char MAKE_CTRL_RIGHT[] = {0xE0, 0x14};
static const unsigned char BREAK_CTRL_RIGHT[] = {0xE0, 0xF0, 0x14};

static const unsigned char MAKE_GUI_RIGHT[] = {0xE0, 0x27};
static const unsigned char BREAK_GUI_RIGHT[] = {0xE0, 0xF0, 0x27};

static const unsigned char MAKE_ALT_RIGHT[] = {0xE0, 0x11};
static const unsigned char BREAK_ALT_RIGHT[] = {0xE0, 0xF0, 0x11};

static const unsigned char MAKE_APPS[] = {0xE0, 0x2F};
static const unsigned char BREAK_APPS[] = {0xE0, 0xF0, 0x2F};

static const unsigned char MAKE_ENTER[] = {0x5A};
static const unsigned char BREAK_ENTER[] = {0xF0, 0x5A};

static const unsigned char MAKE_ESC[] = {0x76};
static const unsigned char BREAK_ESC[] = {0xF0, 0x76};

static const unsigned char MAKE_F1[] = {0x05};
static const unsigned char BREAK_F1[] = {0xF0, 0x05};

static const unsigned char MAKE_F2[] = {0x06};
static const unsigned char BREAK_F2[] = {0xF0, 0x06};

static const unsigned char MAKE_F3[] = {0x04};
static const unsigned char BREAK_F3[] = {0xF0, 0x04};

static const unsigned char MAKE_F4[] = {0x0C};
static const unsigned char BREAK_F4[] = {0xF0, 0x0C};

static const unsigned char MAKE_F5[] = {0x03};
static const unsigned char BREAK_F5[] = {0xF0, 0x03};

static const unsigned char MAKE_F6[] = {0x0B};
static const unsigned char BREAK_F6[] = {0xF0, 0x0B};

static const unsigned char MAKE_F7[] = {0x83};
static const unsigned char BREAK_F7[] = {0xF0, 0x83};

static const unsigned char MAKE_F8[] = {0x0A};
static const unsigned char BREAK_F8[] = {0xF0, 0x0A};

static const unsigned char MAKE_F9[] = {0x01};
static const unsigned char BREAK_F9[] = {0xF0, 0x01};

static const unsigned char MAKE_F10[] = {0x09};
static const unsigned char BREAK_F10[] = {0xF0, 0x09};

static const unsigned char MAKE_F11[] = {0x78};
static const unsigned char BREAK_F11[] = {0xF0, 0x78};

static const unsigned char MAKE_F12[] = {0x07};
static const unsigned char BREAK_F12[] = {0xF0, 0x07};

static const unsigned char MAKE_PRINT_SCREEN[] = {0xE0, 0x12, 0xE0, 0x7C};
static const unsigned char BREAK_PRINT_SCREEN[] = {0xE0, 0xF0, 0x7C, 0xE0, 0xF0, 0x12};

static const unsigned char MAKE_SCROLL_LOCK[] = {0x7E};
static const unsigned char BREAK_SCROLL_LOCK[] = {0xF0, 0x7E};

static const unsigned char MAKE_PAUSE_BREAK[] = {0xE1, 0x14, 0x77, 0xE1, 0xF0, 0x14, 0xF0, 0x77};

static const unsigned char MAKE_LEFT_BRACKET[] = {0x54};
static const unsigned char BREAK_LEFT_BRACKET[] = {0xF0, 0x54};

static const unsigned char MAKE_INSERT[] = {0xE0, 0x70};
static const unsigned char BREAK_INSERT[] = {0xE0, 0xF0, 0x70};

static const unsigned char MAKE_HOME[] = {0xE0, 0x6C};
static const unsigned char BREAK_HOME[] = {0xE0, 0xF0, 0x6C};

static const unsigned char MAKE_PAGE_UP[] = {0xE0, 0x7D};
static const unsigned char BREAK_PAGE_UP[] = {0xE0, 0xF0, 0x7D};

static const unsigned char MAKE_DELETE[] = {0x71};
static const unsigned char BREAK_DELETE[] = {0xF0, 0x71};

static const unsigned char MAKE_END[] = {0xE0, 0x69};
static const unsigned char BREAK_END[] = {0xE0, 0xF0, 0x69};

static const unsigned char MAKE_PAGE_DOWN[] = {0xE0, 0x7A};
static const unsigned char BREAK_PAGE_DOWN[] = {0xE0, 0xF0, 0x7A};

static const unsigned char MAKE_UP_ARROW[] = {0xE0, 0x75};
static const unsigned char BREAK_UP_ARROW[] = {0xE0, 0xF0, 0x75};

static const unsigned char MAKE_LEFT_ARROW[] = {0xE0, 0x6B};
static const unsigned char BREAK_LEFT_ARROW[] = {0xE0, 0xF0, 0x6B};

static const unsigned char MAKE_DOWN_ARROW[] = {0xE0, 0x72};
static const unsigned char BREAK_DOWN_ARROW[] = {0xE0, 0xF0, 0x72};

static const unsigned char MAKE_RIGHT_ARROW[] = {0xE0, 0x74};
static const unsigned char BREAK_RIGHT_ARROW[] = {0xE0, 0xF0, 0x74};

static const unsigned char MAKE_NUM_LOCK[] = {0x77};
static const unsigned char BREAK_NUM_LOCK[] = {0xF0, 0x77};

static const unsigned char MAKE_KEYPAD_SLASH[] = {0xE0, 0x4A};
static const unsigned char BREAK_KEYPAD_SLASH[] = {0xE0, 0xF0, 0x4A};

static const unsigned char MAKE_KEYPAD_ASTERISK[] = {0x7C};
static const unsigned char BREAK_KEYPAD_ASTERISK[] = {0xF0, 0x7C};

static const unsigned char MAKE_KEYPAD_MINUS[] = {0x7B};
static const unsigned char BREAK_KEYPAD_MINUS[] = {0xF0, 0x7B};

static const unsigned char MAKE_KEYPAD_PLUS[] = {0x79};
static const unsigned char BREAK_KEYPAD_PLUS[] = {0xF0, 0x79};

static const unsigned char MAKE_KEYPAD_ENTER[] = {0xE0, 0x5A};
static const unsigned char BREAK_KEYPAD_ENTER[] = {0xE0, 0xF0, 0x5A};

static const unsigned char MAKE_KEYPAD_EN[] = {0xE0, 0x5A};
static const unsigned char BREAK_KEYPAD_EN[] = {0xE0, 0xF0, 0x5A};

static const unsigned char MAKE_KEYPAD_PERIOD[] = {0x71};
static const unsigned char BREAK_KEYPAD_PERIOD[] = {0xF0, 0x71};

static const unsigned char MAKE_KEYPAD_0[] = {0x70};
static const unsigned char BREAK_KEYPAD_0[] = {0xF0, 0x70};

static const unsigned char MAKE_KEYPAD_1[] = {0x69};
static const unsigned char BREAK_KEYPAD_1[] = {0xF0, 0x69};

static const unsigned char MAKE_KEYPAD_2[] = {0x72};
static const unsigned char BREAK_KEYPAD_2[] = {0xF0, 0x72};

static const unsigned char MAKE_KEYPAD_3[] = {0x7A};
static const unsigned char BREAK_KEYPAD_3[] = {0xF0, 0x7A};

static const unsigned char MAKE_KEYPAD_4[] = {0x6B};
static const unsigned char BREAK_KEYPAD_4[] = {0xF0, 0x6B};

static const unsigned char MAKE_KEYPAD_5[] = {0x73};
static const unsigned char BREAK_KEYPAD_5[] = {0xF0, 0x73};

static const unsigned char MAKE_KEYPAD_6[] = {0x74};
static const unsigned char BREAK_KEYPAD_6[] = {0xF0, 0x74};

static const unsigned char MAKE_KEYPAD_7[] = {0x6C};
static const unsigned char BREAK_KEYPAD_7[] = {0xF0, 0x6C};

static const unsigned char MAKE_KEYPAD_8[] = {0x75};
static const unsigned char BREAK_KEYPAD_8[] = {0xF0, 0x75};

static const unsigned char MAKE_KEYPAD_9[] = {0x7D};
static const unsigned char BREAK_KEYPAD_9[] = {0xF0, 0x7D};

static const unsigned char MAKE_RIGHT_BRACKET[] = {0x5B};
static const unsigned char BREAK_RIGHT_BRACKET[] = {0xF0, 0x5B};

static const unsigned char MAKE_SEMICOLON[] = {0x4C};
static const unsigned char BREAK_SEMICOLON[] = {0xF0, 0x4C};

static const unsigned char MAKE_QUOTE[] = {0x52};
static const unsigned char BREAK_QUOTE[] = {0xF0, 0x52};

static const unsigned char MAKE_COMMA[] = {0x41};
static const unsigned char BREAK_COMMA[] = {0xF0, 0x41};

static const unsigned char MAKE_PERIOD[] = {0x49};
static const unsigned char BREAK_PERIOD[] = {0xF0, 0x49};

static const unsigned char MAKE_SLASH[] = {0x4A};
static const unsigned char BREAK_SLASH[] = {0xF0, 0x4A};

static const unsigned char MAKE_POWER[] = {0xE0, 0x37};
static const unsigned char BREAK_POWER[] = {0xE0, 0xF0, 0x37};

static const unsigned char MAKE_SLEEP[] = {0xE0, 0x3F};
static const unsigned char BREAK_SLEEP[] = {0xE0, 0xF0, 0x3F};

static const unsigned char MAKE_WAKE[] = {0xE0, 0x5E};
static const unsigned char BREAK_WAKE[] = {0xE0, 0xF0, 0x5E};

static const unsigned char MAKE_NEXT_TRACK[] = {0xE0, 0x4D};
static const unsigned char BREAK_NEXT_TRACK[] = {0xE0, 0xF0, 0x4D};

static const unsigned char MAKE_PREVIOUS_TRACK[] = {0xE0, 0x15};
static const unsigned char BREAK_PREVIOUS_TRACK[] = {0xE0, 0xF0, 0x15};

static const unsigned char MAKE_STOP[] = {0xE0, 0x3B};
static const unsigned char BREAK_STOP[] = {0xE0, 0xF0, 0x3B};

static const unsigned char MAKE_PLAY_PAUSE[] = {0xE0, 0x34};
static const unsigned char BREAK_PLAY_PAUSE[] = {0xE0, 0xF0, 0x34};

static const unsigned char MAKE_MUTE[] = {0xE0, 0x23};
static const unsigned char BREAK_MUTE[] = {0xE0, 0xF0, 0x23};

static const unsigned char MAKE_VOLUME_UP[] = {0xE0, 0x30};
static const unsigned char BREAK_VOLUME_UP[] = {0xE0, 0xF0, 0x30};

static const unsigned char MAKE_VOLUME_DOWN[] = {0xE0, 0x2E};
static const unsigned char BREAK_VOLUME_DOWN[] = {0xE0, 0xF0, 0x2E};

static const unsigned char MAKE_MEDIA_SELECT[] = {0xE0, 0x50};
static const unsigned char BREAK_MEDIA_SELECT[] = {0xE0, 0xF0, 0x50};

static const unsigned char MAKE_EMAIL[] = {0xE0, 0x48};
static const unsigned char BREAK_EMAIL[] = {0xE0, 0xF0, 0x48};

static const unsigned char MAKE_CALCULATOR[] = {0xE0, 0x2B};
static const unsigned char BREAK_CALCULATOR[] = {0xE0, 0xF0, 0x2B};

static const unsigned char MAKE_MY_COMPUTER[] = {0xE0, 0x40};
static const unsigned char BREAK_MY_COMPUTER[] = {0xE0, 0xF0, 0x40};

static const unsigned char MAKE_WWW_SEARCH[] = {0xE0, 0x10};
static const unsigned char BREAK_WWW_SEARCH[] = {0xE0, 0xF0, 0x10};

static const unsigned char MAKE_WWW_HOME[] = {0xE0, 0x3A};
static const unsigned char BREAK_WWW_HOME[] = {0xE0, 0xF0, 0x3A};

static const unsigned char MAKE_WWW_BACK[] = {0xE0, 0x38};
static const unsigned char BREAK_WWW_BACK[] = {0xE0, 0xF0, 0x38};

static const unsigned char MAKE_WWW_FORWARD[] = {0xE0, 0x30};
static const unsigned char BREAK_WWW_FORWARD[] = {0xE0, 0xF0, 0x30};

static const unsigned char MAKE_WWW_STOP[] = {0xE0, 0x28};
static const unsigned char BREAK_WWW_STOP[] = {0xE0, 0xF0, 0x28};

static const unsigned char MAKE_WWW_REFRESH[] = {0xE0, 0x20};
static const unsigned char BREAK_WWW_REFRESH[] = {0xE0, 0xF0, 0x20};

static const unsigned char MAKE_WWW_FAVORITES[] = {0xE0, 0x18};
static const unsigned char BREAK_WWW_FAVORITES[] = {0xE0, 0xF0, 0x18};

#define KEY_ENTRY(bytes, ascii) {bytes, sizeof(bytes), #bytes, ascii}
// ascii = '\0' no ascii representation
key_table_t keytable[] = { // clang-format off
    KEY_ENTRY(MAKE_A, 'a'),
    KEY_ENTRY(MAKE_B, 'b'),
    KEY_ENTRY(MAKE_C, 'c'),
    KEY_ENTRY(MAKE_D, 'd'),
    KEY_ENTRY(MAKE_E, 'e'),
    KEY_ENTRY(MAKE_F, 'f'),
    KEY_ENTRY(MAKE_G, 'g'),
    KEY_ENTRY(MAKE_H, 'h'),
    KEY_ENTRY(MAKE_I, 'i'),
    KEY_ENTRY(MAKE_J, 'j'),
    KEY_ENTRY(MAKE_K, 'k'),
    KEY_ENTRY(MAKE_L, 'l'),
    KEY_ENTRY(MAKE_M, 'm'),
    KEY_ENTRY(MAKE_N, 'n'),
    KEY_ENTRY(MAKE_O, 'o'),
    KEY_ENTRY(MAKE_P, 'p'),
    KEY_ENTRY(MAKE_Q, 'q'),
    KEY_ENTRY(MAKE_R, 'r'),
    KEY_ENTRY(MAKE_S, 's'),
    KEY_ENTRY(MAKE_T, 't'),
    KEY_ENTRY(MAKE_U, 'u'),
    KEY_ENTRY(MAKE_V, 'v'),
    KEY_ENTRY(MAKE_W, 'w'),
    KEY_ENTRY(MAKE_X, 'x'),
    KEY_ENTRY(MAKE_Y, 'y'),
    KEY_ENTRY(MAKE_Z, 'z'),
    KEY_ENTRY(MAKE_0, '0'),
    KEY_ENTRY(MAKE_1, '1'),
    KEY_ENTRY(MAKE_2, '2'),
    KEY_ENTRY(MAKE_3, '3'),
    KEY_ENTRY(MAKE_4, '4'),
    KEY_ENTRY(MAKE_5, '5'),
    KEY_ENTRY(MAKE_6, '6'),
    KEY_ENTRY(MAKE_7, '7'),
    KEY_ENTRY(MAKE_8, '8'),
    KEY_ENTRY(MAKE_9, '9'),
    KEY_ENTRY(MAKE_BACKTICK, '`'),
    KEY_ENTRY(MAKE_DASH, '-'),
    KEY_ENTRY(MAKE_EQUAL, '='),
    KEY_ENTRY(MAKE_BACKSLASH, '\\'),
    KEY_ENTRY(MAKE_BACKSPACE, '\b'),
    KEY_ENTRY(MAKE_SPACE, ' '),
    KEY_ENTRY(MAKE_TAB, '\t'),
    KEY_ENTRY(MAKE_CAPSLOCK, '\0'),
    KEY_ENTRY(MAKE_SHIFT_LEFT, '\0'),
    KEY_ENTRY(MAKE_CTRL_LEFT, '\0'),
    KEY_ENTRY(MAKE_GUI_LEFT, '\0'),
    KEY_ENTRY(MAKE_ALT_LEFT, '\0'),
    KEY_ENTRY(MAKE_SHIFT_RIGHT, '\0'),
    KEY_ENTRY(MAKE_CTRL_RIGHT, '\0'),
    KEY_ENTRY(MAKE_GUI_RIGHT, '\0'),
    KEY_ENTRY(MAKE_ALT_RIGHT, '\0'),
    KEY_ENTRY(MAKE_APPS, '\0'),
    KEY_ENTRY(MAKE_ENTER, '\n'),
    KEY_ENTRY(MAKE_ESC, '\0'),
    KEY_ENTRY(MAKE_F1, '\0'),
    KEY_ENTRY(MAKE_F2, '\0'),
    KEY_ENTRY(MAKE_F3, '\0'),
    KEY_ENTRY(MAKE_F4, '\0'),
    KEY_ENTRY(MAKE_F5, '\0'),
    KEY_ENTRY(MAKE_F6, '\0'),
    KEY_ENTRY(MAKE_F7, '\0'),
    KEY_ENTRY(MAKE_F8, '\0'),
    KEY_ENTRY(MAKE_F9, '\0'),
    KEY_ENTRY(MAKE_F10, '\0'),
    KEY_ENTRY(MAKE_F11, '\0'),
    KEY_ENTRY(MAKE_F12, '\0'),
    KEY_ENTRY(MAKE_PRINT_SCREEN, '\0'),
    KEY_ENTRY(MAKE_SCROLL_LOCK, '\0'),
    KEY_ENTRY(MAKE_PAUSE_BREAK, '\0'),
    KEY_ENTRY(MAKE_LEFT_BRACKET, '['),
    KEY_ENTRY(MAKE_INSERT, '\0'),
    KEY_ENTRY(MAKE_HOME, '\0'),
    KEY_ENTRY(MAKE_PAGE_UP, '\0'),
    KEY_ENTRY(MAKE_DELETE, '\0'),
    KEY_ENTRY(MAKE_END, '\0'),
    KEY_ENTRY(MAKE_PAGE_DOWN, '\0'),
    KEY_ENTRY(MAKE_UP_ARROW, '\0'),
    KEY_ENTRY(MAKE_LEFT_ARROW, '\0'),
    KEY_ENTRY(MAKE_DOWN_ARROW, '\0'),
    KEY_ENTRY(MAKE_RIGHT_ARROW, '\0'),
    KEY_ENTRY(MAKE_NUM_LOCK, '\0'),
    KEY_ENTRY(MAKE_KEYPAD_SLASH, '/'),
    KEY_ENTRY(MAKE_KEYPAD_ASTERISK, '*'),
    KEY_ENTRY(MAKE_KEYPAD_MINUS, '-'),
    KEY_ENTRY(MAKE_KEYPAD_PLUS, '+'),
    KEY_ENTRY(MAKE_KEYPAD_ENTER, '\n'),
    KEY_ENTRY(MAKE_KEYPAD_EN, '\0'),
    KEY_ENTRY(MAKE_KEYPAD_PERIOD, '.'),
    KEY_ENTRY(MAKE_KEYPAD_0, '0'),
    KEY_ENTRY(MAKE_KEYPAD_1, '1'),
    KEY_ENTRY(MAKE_KEYPAD_2, '2'),
    KEY_ENTRY(MAKE_KEYPAD_3, '3'),
    KEY_ENTRY(MAKE_KEYPAD_4, '4'),
    KEY_ENTRY(MAKE_KEYPAD_5, '5'),
    KEY_ENTRY(MAKE_KEYPAD_6, '6'),
    KEY_ENTRY(MAKE_KEYPAD_7, '7'),
    KEY_ENTRY(MAKE_KEYPAD_8, '8'),
    KEY_ENTRY(MAKE_KEYPAD_9, '9'),
    KEY_ENTRY(MAKE_RIGHT_BRACKET, ']'),
    KEY_ENTRY(MAKE_SEMICOLON, ';'),
    KEY_ENTRY(MAKE_QUOTE, '\''),
    KEY_ENTRY(MAKE_COMMA, ','),
    KEY_ENTRY(MAKE_PERIOD, '.'),
    KEY_ENTRY(MAKE_SLASH, '/'),
    KEY_ENTRY(MAKE_POWER, '\0'),
    KEY_ENTRY(MAKE_SLEEP, '\0'),
    KEY_ENTRY(MAKE_WAKE, '\0'),
    KEY_ENTRY(MAKE_NEXT_TRACK, '\0'),
    KEY_ENTRY(MAKE_PREVIOUS_TRACK, '\0'),
    KEY_ENTRY(MAKE_STOP, '\0'),
    KEY_ENTRY(MAKE_PLAY_PAUSE, '\0'),
    KEY_ENTRY(MAKE_MUTE, '\0'),
    KEY_ENTRY(MAKE_VOLUME_UP, '\0'),
    KEY_ENTRY(MAKE_VOLUME_DOWN, '\0'),
    KEY_ENTRY(MAKE_MEDIA_SELECT, '\0'),
    KEY_ENTRY(MAKE_EMAIL, '\0'),
    KEY_ENTRY(MAKE_CALCULATOR, '\0'),
    KEY_ENTRY(MAKE_MY_COMPUTER, '\0'),
    KEY_ENTRY(MAKE_WWW_SEARCH, '\0'),
    KEY_ENTRY(MAKE_WWW_HOME, '\0'),
    KEY_ENTRY(MAKE_WWW_BACK, '\0'),
    KEY_ENTRY(MAKE_WWW_FORWARD, '\0'),
    KEY_ENTRY(MAKE_WWW_STOP, '\0'),
    KEY_ENTRY(MAKE_WWW_REFRESH, '\0'),
    KEY_ENTRY(MAKE_WWW_FAVORITES, '\0'),

    KEY_ENTRY(BREAK_A, 'a'),
    KEY_ENTRY(BREAK_B, 'b'),
    KEY_ENTRY(BREAK_C, 'c'),
    KEY_ENTRY(BREAK_D, 'd'),
    KEY_ENTRY(BREAK_E, 'e'),
    KEY_ENTRY(BREAK_F, 'f'),
    KEY_ENTRY(BREAK_G, 'g'),
    KEY_ENTRY(BREAK_H, 'h'),
    KEY_ENTRY(BREAK_I, 'i'),
    KEY_ENTRY(BREAK_J, 'j'),
    KEY_ENTRY(BREAK_K, 'k'),
    KEY_ENTRY(BREAK_L, 'l'),
    KEY_ENTRY(BREAK_M, 'm'),
    KEY_ENTRY(BREAK_N, 'n'),
    KEY_ENTRY(BREAK_O, 'o'),
    KEY_ENTRY(BREAK_P, 'p'),
    KEY_ENTRY(BREAK_Q, 'q'),
    KEY_ENTRY(BREAK_R, 'r'),
    KEY_ENTRY(BREAK_S, 's'),
    KEY_ENTRY(BREAK_T, 't'),
    KEY_ENTRY(BREAK_U, 'u'),
    KEY_ENTRY(BREAK_V, 'v'),
    KEY_ENTRY(BREAK_W, 'w'),
    KEY_ENTRY(BREAK_X, 'x'),
    KEY_ENTRY(BREAK_Y, 'y'),
    KEY_ENTRY(BREAK_Z, 'z'),
    KEY_ENTRY(BREAK_0, '0'),
    KEY_ENTRY(BREAK_1, '1'),
    KEY_ENTRY(BREAK_2, '2'),
    KEY_ENTRY(BREAK_3, '3'),
    KEY_ENTRY(BREAK_4, '4'),
    KEY_ENTRY(BREAK_5, '5'),
    KEY_ENTRY(BREAK_6, '6'),
    KEY_ENTRY(BREAK_7, '7'),
    KEY_ENTRY(BREAK_8, '8'),
    KEY_ENTRY(BREAK_9, '9'),
    KEY_ENTRY(BREAK_BACKTICK, '`'),
    KEY_ENTRY(BREAK_DASH, '-'),
    KEY_ENTRY(BREAK_EQUAL, '='),
    KEY_ENTRY(BREAK_BACKSLASH, '\\'),
    KEY_ENTRY(BREAK_BACKSPACE, '\b'),
    KEY_ENTRY(BREAK_SPACE, ' '),
    KEY_ENTRY(BREAK_TAB, '\t'),
    KEY_ENTRY(BREAK_CAPSLOCK, '\0'),
    KEY_ENTRY(BREAK_SHIFT_LEFT, '\0'),
    KEY_ENTRY(BREAK_CTRL_LEFT, '\0'),
    KEY_ENTRY(BREAK_GUI_LEFT, '\0'),
    KEY_ENTRY(BREAK_ALT_LEFT, '\0'),
    KEY_ENTRY(BREAK_SHIFT_RIGHT, '\0'),
    KEY_ENTRY(BREAK_CTRL_RIGHT, '\0'),
    KEY_ENTRY(BREAK_GUI_RIGHT, '\0'),
    KEY_ENTRY(BREAK_ALT_RIGHT, '\0'),
    KEY_ENTRY(BREAK_APPS, '\0'),
    KEY_ENTRY(BREAK_ENTER, '\n'),
    KEY_ENTRY(BREAK_ESC, '\0'),
    KEY_ENTRY(BREAK_F1, '\0'),
    KEY_ENTRY(BREAK_F2, '\0'),
    KEY_ENTRY(BREAK_F3, '\0'),
    KEY_ENTRY(BREAK_F4, '\0'),
    KEY_ENTRY(BREAK_F5, '\0'),
    KEY_ENTRY(BREAK_F6, '\0'),
    KEY_ENTRY(BREAK_F7, '\0'),
    KEY_ENTRY(BREAK_F8, '\0'),
    KEY_ENTRY(BREAK_F9, '\0'),
    KEY_ENTRY(BREAK_F10, '\0'),
    KEY_ENTRY(BREAK_F11, '\0'),
    KEY_ENTRY(BREAK_F12, '\0'),
    KEY_ENTRY(BREAK_PRINT_SCREEN, '\0'),
    KEY_ENTRY(BREAK_SCROLL_LOCK, '\0'),
    KEY_ENTRY(BREAK_LEFT_BRACKET, '['),
    KEY_ENTRY(BREAK_INSERT, '\0'),
    KEY_ENTRY(BREAK_HOME, '\0'),
    KEY_ENTRY(BREAK_PAGE_UP, '\0'),
    KEY_ENTRY(BREAK_DELETE, '\0'),
    KEY_ENTRY(BREAK_END, '\0'),
    KEY_ENTRY(BREAK_PAGE_DOWN, '\0'),
    KEY_ENTRY(BREAK_UP_ARROW, '\0'),
    KEY_ENTRY(BREAK_LEFT_ARROW, '\0'),
    KEY_ENTRY(BREAK_DOWN_ARROW, '\0'),
    KEY_ENTRY(BREAK_RIGHT_ARROW, '\0'),
    KEY_ENTRY(BREAK_NUM_LOCK, '\0'),
    KEY_ENTRY(BREAK_KEYPAD_SLASH, '/'),
    KEY_ENTRY(BREAK_KEYPAD_ASTERISK, '*'),
    KEY_ENTRY(BREAK_KEYPAD_MINUS, '-'),
    KEY_ENTRY(BREAK_KEYPAD_PLUS, '+'),
    KEY_ENTRY(BREAK_KEYPAD_ENTER, '\n'),
    KEY_ENTRY(BREAK_KEYPAD_EN, '\0'),
    KEY_ENTRY(BREAK_KEYPAD_PERIOD, '.'),
    KEY_ENTRY(BREAK_KEYPAD_0, '0'),
    KEY_ENTRY(BREAK_KEYPAD_1, '1'),
    KEY_ENTRY(BREAK_KEYPAD_2, '2'),
    KEY_ENTRY(BREAK_KEYPAD_3, '3'),
    KEY_ENTRY(BREAK_KEYPAD_4, '4'),
    KEY_ENTRY(BREAK_KEYPAD_5, '5'),
    KEY_ENTRY(BREAK_KEYPAD_6, '6'),
    KEY_ENTRY(BREAK_KEYPAD_7, '7'),
    KEY_ENTRY(BREAK_KEYPAD_8, '8'),
    KEY_ENTRY(BREAK_KEYPAD_9, '9'),
    KEY_ENTRY(BREAK_RIGHT_BRACKET, ']'),
    KEY_ENTRY(BREAK_SEMICOLON, ';'),
    KEY_ENTRY(BREAK_QUOTE, '\''),
    KEY_ENTRY(BREAK_COMMA, ','),
    KEY_ENTRY(BREAK_PERIOD, '.'),
    KEY_ENTRY(BREAK_SLASH, '/'),
    KEY_ENTRY(BREAK_POWER, '\0'),
    KEY_ENTRY(BREAK_SLEEP, '\0'),
    KEY_ENTRY(BREAK_WAKE, '\0'),
    KEY_ENTRY(BREAK_NEXT_TRACK, '\0'),
    KEY_ENTRY(BREAK_PREVIOUS_TRACK, '\0'),
    KEY_ENTRY(BREAK_STOP, '\0'),
    KEY_ENTRY(BREAK_PLAY_PAUSE, '\0'),
    KEY_ENTRY(BREAK_MUTE, '\0'),
    KEY_ENTRY(BREAK_VOLUME_UP, '\0'),
    KEY_ENTRY(BREAK_VOLUME_DOWN, '\0'),
    KEY_ENTRY(BREAK_MEDIA_SELECT, '\0'),
    KEY_ENTRY(BREAK_EMAIL, '\0'),
    KEY_ENTRY(BREAK_CALCULATOR, '\0'),
    KEY_ENTRY(BREAK_MY_COMPUTER, '\0'),
    KEY_ENTRY(BREAK_WWW_SEARCH, '\0'),
    KEY_ENTRY(BREAK_WWW_HOME, '\0'),
    KEY_ENTRY(BREAK_WWW_BACK, '\0'),
    KEY_ENTRY(BREAK_WWW_FORWARD, '\0'),
    KEY_ENTRY(BREAK_WWW_STOP, '\0'),
    KEY_ENTRY(BREAK_WWW_REFRESH, '\0'),
    KEY_ENTRY(BREAK_WWW_FAVORITES, '\0')
}; // clang-format on

static const key_table_t *match_sequence(const unsigned char *buf, size_t len, int *matches, int *exact_index)
{
    *matches = 0;
    *exact_index = -1;

    for (int i = 0; i < (int)(sizeof(keytable) / sizeof(keytable[0])); i++)
    {
        const key_table_t *entry = &keytable[i];
        if (len <= entry->num_bytes)
        {
            int cmp = memcmp(entry->bytes, buf, len);
            if (cmp == 0)
            {
                (*matches)++;
                if (len == entry->num_bytes)
                {
                    *exact_index = i;
                }
            }
        }
    }
    if (*matches == 1 && *exact_index != -1)
    {
        return &keytable[*exact_index];
    }
    return NULL;
}

const key_table_t *get_make_code(unsigned char new_byte)
{
    if (parser_state.length < sizeof(parser_state.buffer))
    {
        parser_state.buffer[parser_state.length++] = new_byte;
    }
    else
    {
        parser_state.length = 0;
        parser_state.buffer[parser_state.length++] = new_byte;
    }

    int matches = 0, exact_index = -1;
    const key_table_t *found =
        match_sequence((const unsigned char *)parser_state.buffer, parser_state.length, &matches, &exact_index);

    if (found)
    {
        parser_state.length = 0;
        return found;
    }
    else
    {
        if (matches == 0)
        {
            parser_state.length = 0;
            return NULL;
        }
        else
        {
            return NULL;
        }
    }
}

static void free_all_pio_state_machines(PIO pio)
{
    for (int sm = 0; sm < 4; sm++)
    {
        if (pio_sm_is_claimed(pio, sm))
        {
            pio_sm_unclaim(pio, sm);
        }
    }
}

static void pio_destroy(void)
{
    free_all_pio_state_machines(pio0);
    free_all_pio_state_machines(pio1);
    pio_clear_instruction_memory(pio0);
    pio_clear_instruction_memory(pio1);
}

static void restart_device_to_host_sm(void)
{
    pio_sm_exec(pio0, kbd_sm, pio_encode_jmp(offset_kbd));
}

static void restart_host_to_device_sm(void)
{
    pio_sm_exec(pio1, kbd_h2d_sm, pio_encode_jmp(offset_kbd_h2d));
}

static void stop_device_to_host_sm(void)
{
    bio_put(BIO_AUX_D2H_JMP_GPIO, true);
    while (!gpio_get(AUX_D2H_JMP_GPIO))
    {
        tight_loop_contents();
    }
    restart_device_to_host_sm();
}

static void start_device_to_host_sm(void)
{
    bio_put(BIO_AUX_D2H_JMP_GPIO, false);
    restart_device_to_host_sm();
}

// IRQ0: Inhibited communication detected
// IRQ1: No Host Request-to-Send detected after inhibiting communication
void pio0_irq(void)
{
    //printf("\r\nPIO0 IRQ!\r\n");
    if (pio0_hw->irq & 1)
    {
        //printf("PIO0 IRQ & 1: %d\r\n", inhnr++);
        pio0_hw->irq = 1;
        stop_device_to_host_sm();
    }
    else if (pio0_hw->irq & 2)
    {
        //printf("PIO0 IRQ & 2: %d\r\n", inhnr++);
        pio0_hw->irq = 2;
        start_device_to_host_sm();
        restart_host_to_device_sm();
    }
}

// IRQ0: IDLE DETECTED, CLOCK is HIGH + DAT is HIGH for at least 100 microseconds
void pio1_irq(void)
{
    //printf("\r\nPIO1 IRQ!\r\n");
    if (pio1_hw->irq & 1)
    {
        //printf("PIO1 IRQ & 1: %d\r\n", inidle++);
        pio1_hw->irq = 1;
        start_device_to_host_sm();
        restart_host_to_device_sm();
    }
    else if (pio1_hw->irq & 2)
    {
        //printf("PIO1 IRQ & 2: %d\r\n", inhnr++);
        pio1_hw->irq = 2;
    }
}

void core1_main()
{
    volatile static unsigned char buffer_keys[32 * 10] = {0};
    unsigned int buffer_keys_index = 0;
    unsigned int total_pio_packets = 0;
    unsigned int read_index = 0;

    memset((void *)buffer_keys, 0, sizeof(buffer_keys));
    memset((void *)&parser_state, 0, sizeof(parser_state));

    while (1)
    {
        while (read_index != write_index)
        {
            const ps2_packet_t *packet =
                (const ps2_packet_t *)&(ringbuff[read_index++ % (RING_BUFF_MAX_ENTRIES - 1)][32]);
            printf("[0x%04X] t:0x%08X %c:0x%02X\r\n", total_pio_packets, packet->timestamp,
                   packet->device_to_host ? 'D' : 'H', packet->byte);
            total_pio_packets++;
            if (packet->device_to_host && packet->byte)
            {
                const key_table_t *entry = get_make_code(packet->byte);
                if (entry)
                {
                    printf("%s", entry->name);
                    if (entry->ascii)
                    {
                        printf(" --> %c", entry->ascii);
                        if (strstr(entry->name, "BREAK"))
                        {
                            buffer_keys[buffer_keys_index++ % (sizeof(buffer_keys) - 1)] = entry->ascii;
                            printf("\r\n**********\r\nkey break tracker (limit 0x%02X bytes): %s\r\n**********",
                                   sizeof(buffer_keys) - 1, buffer_keys);
                        }
                    }
                    printf("\r\n-----------\r\n");
                }
            }
        }
    }
}

/*
   printf("poc!\r\n");


*/

void pio_total_destroy_bp(void)
{
    for (int pio_index = 0; pio_index < NUM_PIOS; pio_index++) {
        PIO pio = pio_index == 0 ? pio0 :
                  pio_index == 1 ? pio1 :
#if NUM_PIOS > 2
                  pio_index == 2 ? pio2 :
#endif
                  NULL;
        if (pio == NULL) break;
        printf("pio_total_destroy_bp: %d\r\n", pio_index);
        for (int sm = 0; sm < 4; sm++) {
            if (pio_sm_is_claimed(pio, sm)) {
                printf("pio_total_destroy_bp: %d sm: %d\r\n", pio_index, sm);
                pio_sm_unclaim(pio, sm);
            }
        }
        pio_clear_instruction_memory(pio);
    }
}


void ps2_handler(struct command_result* res) {
    if (NULL == ringbuff)
    {
        printf("Buffer not allocated\r\n");
        return;
    }

    uint8_t args[] = { 0x05, 0x00, 0x01, 0x90 }; // 5V
    //binmode_debug = 1;
    uint32_t result = binmode_psu_enable(args);

    if (result) {
            printf("\r\nPSU ERROR CODE %d\r\n", result);
    } else {
            printf("\r\nPSU 5v Enabled\r\n");
        }

    printf("\r\npico-ps2-sniffer started! for Bus Pirate\r\n"
        "https://github.com/therealdreg/pico-ps2-sniffer\r\n"
        "MIT License David Reguera Garcia aka Dreg\r\n"
        "---------------------------------------------------------------\r\n"
        "\r\n");

    bio_input(BIO_DAT_GPIO); // DAT
    bio_input(BIO_CLK_GPIO); // CLOCK

    bio_output(BIO_AUX_D2H_JMP_GPIO); // AUX D2H JMP GPIO
    bio_output(BIO_AUX_H2D_JMP_GPIO); // AUX H2D JMP GPIO

    bio_put(BIO_AUX_D2H_JMP_GPIO, false);
    bio_put(BIO_AUX_H2D_JMP_GPIO, true);

    binmode_pullup_enable(args);

    pio_destroy();

    char cmd;
    while (rx_fifo_try_get(&cmd))
    {
        busy_wait_ms(10);
    }

    printf("\r\nPress 'q' to exit\r\n\r\nSniffing...\r\n\r\n");

    // host to device:
    // get a state machine
    kbd_h2d_sm = pio_claim_unused_sm(pio1, true);
    // reserve program space in SM memory
    offset_kbd_h2d = pio_add_program(pio1, &host_to_device_program);
    //printf("kbd_h2d_sm: %d offset_kbd_h2d: %d\r\n", kbd_h2d_sm, offset_kbd_h2d);
    // Set pin directions base
    pio_sm_set_consecutive_pindirs(pio1, kbd_sm, DAT_GPIO, 2, false);
    // program the start and wrap SM registers
    pio_sm_config c_kbd_h2d = device_to_host_program_get_default_config(offset_kbd_h2d);
    // Set the base input pin. pin index 0 is DAT, index 1 is CLK
    sm_config_set_in_pins(&c_kbd_h2d, DAT_GPIO);
    // Shift 8 bits to the right, autopush disabled
    sm_config_set_in_shift(&c_kbd_h2d, true, false, 0);
    // JMP pin
    sm_config_set_jmp_pin(&c_kbd_h2d, CLK_GPIO);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c_kbd_h2d, PIO_FIFO_JOIN_RX);
    // Must run ~133.6 kHz, 7.5 microseconds per cycle
    // it is expected to have no fewer than 8 PIO state machine cycles for each keyboard clock cycle
    float div_kbd_h2d = (float)clock_get_hz(clk_sys) / (8 * 16700);
    sm_config_set_clkdiv(&c_kbd_h2d, div_kbd_h2d);
    // Initialize the state machine
    pio_sm_init(pio1, kbd_h2d_sm, offset_kbd_h2d, &c_kbd_h2d);
    pio_sm_set_enabled(pio1, kbd_h2d_sm, false);
    pio_sm_clear_fifos(pio1, kbd_h2d_sm);
    pio_sm_restart(pio1, kbd_h2d_sm);
    pio_sm_clkdiv_restart(pio1, kbd_h2d_sm);
    pio_sm_set_enabled(pio1, kbd_h2d_sm, true);
    pio_sm_exec(pio1, kbd_h2d_sm, pio_encode_jmp(offset_kbd_h2d));

    // idle detection:
    // get a state machine
    uint kbd_idle_sm = pio_claim_unused_sm(pio1, true);
    // reserve program space in SM memory
    uint offset_idle = pio_add_program(pio1, &idle_signal_program);
    //printf("kbd_idle_sm: %d offset_idle: %d\r\n", kbd_idle_sm, offset_idle);
    // Set pin directions base
    pio_sm_set_consecutive_pindirs(pio1, kbd_idle_sm, DAT_GPIO, 2, false);
    // program the start and wrap SM registers
    pio_sm_config c_idle = idle_signal_program_get_default_config(offset_idle);
    // Set the base input pin. pin index 0 is DAT
    sm_config_set_in_pins(&c_idle, DAT_GPIO);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c_idle, PIO_FIFO_JOIN_RX);
    // JMP pin (CLOCK)
    sm_config_set_jmp_pin(&c_idle, CLK_GPIO);
    // 1 MHz
    float div_idle = clock_get_hz(clk_sys) / 1000000.0;
    sm_config_set_clkdiv(&c_idle, div_idle);
    // Set IRQ handler
    pio_set_irq0_source_mask_enabled(pio1, 0x0F00, true);
    irq_set_exclusive_handler(PIO1_IRQ_0, pio1_irq);
    irq_set_enabled(PIO1_IRQ_0, true);
    // initialize the state machine
    pio_sm_init(pio1, kbd_idle_sm, offset_idle, &c_idle);
    pio_sm_set_enabled(pio1, kbd_idle_sm, false);
    pio_sm_clear_fifos(pio1, kbd_idle_sm);
    pio_sm_restart(pio1, kbd_idle_sm);
    pio_sm_clkdiv_restart(pio1, kbd_idle_sm);
    pio_sm_set_enabled(pio1, kbd_idle_sm, true);
    pio_sm_exec(pio1, kbd_idle_sm, pio_encode_jmp(offset_idle));

    // inhibited detection:
    // get a state machine
    uint kbd_inh_sm = pio_claim_unused_sm(pio0, true);
    // reserve program space in SM memory
    uint offset_inh = pio_add_program(pio0, &inhibited_signal_program);
    //printf("kbd_inh_sm: %d offset_inh: %d\r\n", kbd_inh_sm, offset_inh);
    // Set pin directions base
    pio_sm_set_consecutive_pindirs(pio0, kbd_inh_sm, DAT_GPIO, 2, false);
    // program the start and wrap SM registers
    pio_sm_config c_inh = inhibited_signal_program_get_default_config(offset_inh);
    // Set the base input pin. pin index 0 is DAT
    sm_config_set_in_pins(&c_inh, DAT_GPIO);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c_inh, PIO_FIFO_JOIN_RX);
    // JMP pin (CLOCK)
    sm_config_set_jmp_pin(&c_inh, CLK_GPIO);
    // 1 MHz
    float div_inh = clock_get_hz(clk_sys) / 1000000.0;
    sm_config_set_clkdiv(&c_inh, div_inh);
    // Set IRQ handler
    pio_set_irq0_source_mask_enabled(pio0, 0x0F00, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq);
    irq_set_enabled(PIO0_IRQ_0, true);
    // initialize the state machine
    pio_sm_init(pio0, kbd_inh_sm, offset_inh, &c_inh);
    pio_sm_set_enabled(pio0, kbd_inh_sm, false);
    pio_sm_clear_fifos(pio0, kbd_inh_sm);
    pio_sm_restart(pio0, kbd_inh_sm);
    pio_sm_clkdiv_restart(pio0, kbd_inh_sm);
    pio_sm_set_enabled(pio0, kbd_inh_sm, true);
    pio_sm_exec(pio0, kbd_inh_sm, pio_encode_jmp(offset_inh));

    // device to host:
    // get a state machine
    kbd_sm = pio_claim_unused_sm(pio0, true);
    // reserve program space in SM memory
    offset_kbd = pio_add_program(pio0, &device_to_host_program);
    //printf("kbd_sm: %d offset_kbd: %d\r\n", kbd_sm, offset_kbd);
    // Set pin directions base
    pio_sm_set_consecutive_pindirs(pio0, kbd_sm, DAT_GPIO, 3, false);
    // program the start and wrap SM registers
    pio_sm_config c_kbd = device_to_host_program_get_default_config(offset_kbd);
    // Set the base input pin. pin index 0 is DAT, index 1 is CLK
    sm_config_set_in_pins(&c_kbd, DAT_GPIO);
    // Shift 8 bits to the right, autopush disabled
    // sm_config_set_in_shift(&c_kbd, true, true, 8);
    sm_config_set_in_shift(&c_kbd, true, false, 0);
    // JMP pin
    sm_config_set_jmp_pin(&c_kbd, AUX_D2H_JMP_GPIO);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c_kbd, PIO_FIFO_JOIN_RX);
    // Must run ~133.6 kHz, 7.5 microseconds per cycle
    // it is expected to have no fewer than 8 PIO state machine cycles for each keyboard clock cycle
    float div_kbd = (float)clock_get_hz(clk_sys) / (8 * 16700);
    sm_config_set_clkdiv(&c_kbd, div_kbd);
    // Initialize the state machine
    pio_sm_init(pio0, kbd_sm, offset_kbd, &c_kbd);
    pio_sm_set_enabled(pio0, kbd_sm, false);
    pio_sm_clear_fifos(pio0, kbd_sm);
    pio_sm_restart(pio0, kbd_sm);
    pio_sm_clkdiv_restart(pio0, kbd_sm);
    pio_sm_set_enabled(pio0, kbd_sm, true);
    pio_sm_exec(pio0, kbd_sm, pio_encode_jmp(offset_kbd));

    start_device_to_host_sm();
    restart_host_to_device_sm();

    while (1)
    {
            /* The pushed value is an 8-bit sample positioned in the upper (most significant) byte of the
                 32-bit FIFO word, In C, you can read this byte from:

            io_rw_8* rxfifo_shift = (io_rw_8*)&pio->rxf[sm] + 3;

            This offset (+3) accesses the top byte of the 32-bit word in which the data resides. */
            if (!pio_sm_is_rx_fifo_empty(pio1, kbd_h2d_sm))
            {
                uint8_t byte = *((io_rw_8 *)&pio1->rxf[kbd_h2d_sm] + 3);
                printf("H2D: 0x%02X\r\n", byte);

                const ps2_packet_t packet = {byte, us_to_ms(time_us_64()), false};
                memcpy((void *)&(ringbuff[write_index % (RING_BUFF_MAX_ENTRIES - 1)][32]), &packet, sizeof(ps2_packet_t));
                write_index++;
            }
            if (!pio_sm_is_rx_fifo_empty(pio0, kbd_sm))
            {
                uint8_t byte = *((io_rw_8 *)&pio0->rxf[kbd_sm] + 3);
                printf("D2H: 0x%02X\r\n", byte);

                const ps2_packet_t packet = {byte, us_to_ms(time_us_64()), true};
                memcpy((void *)&(ringbuff[write_index % (RING_BUFF_MAX_ENTRIES - 1)][32]), &packet, sizeof(ps2_packet_t));
                write_index++;
            }
            if (rx_fifo_try_get(&cmd))
            {
                if (cmd == 'q' || cmd == 'Q')
                {
                    printf("\r\n\r\nexit...\r\n");
                    return;
                }
            }
    }
}


/*        while (1) {
            printf("DAT_GPIO(%d): %d, CLK_GPIO(%d): %d\r\n", DAT_GPIO, bio_get(DAT_GPIO), CLK_GPIO, bio_get(CLK_GPIO));
            sleep_ms(100);
    }
*/

// command configuration
const struct _mode_command_struct ps2_commands[] = {
    {   .command="sniff",
        .func=&ps2_handler,
        .description_text=T_PS2_DESCRIPTION,
        .supress_fala_capture=true
    },

};
const uint32_t ps2_commands_count = count_of(ps2_commands);

static const char pin_labels[][5] = { "TRIG", "D+", "D-", "RESV", "DAT", "CLK", "RESV", "RESV" };

//static struct _spi_mode_config mode_config;

uint32_t ps2_setup(void) {

    return 1;
}

uint32_t ps2_setup_exc(void) {
    bio_init();
    system_bio_update_purpose_and_label(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_MODE, pin_labels[3]);
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_MODE, pin_labels[4]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_MODE, pin_labels[5]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[6]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[7]);

    if (NULL == ringbuff)
    {
        ringbuff = (volatile uint8_t (*)[32]) mem_alloc(RING_BUFF_MAX_ENTRIES * sizeof(*ringbuff), 0);
    }

    return 10000000;
}

void ps2_cleanup(void) {
    if (ringbuff)
    {
        mem_free((uint8_t*)ringbuff);
        ringbuff = NULL;
    }

    // release pin claims
    system_bio_update_purpose_and_label(false, BIO0, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
    bio_init();

    uint8_t binmode_args = 0x00;
    uint32_t result = binmode_psu_disable(&binmode_args);

    pio_destroy();
}

bool ps2_preflight_sanity_check(void){
    ui_help_sanity_check(true, 0x00);
}

void ps2_pins(void){
    printf("TRST\tSRST\tCS\tMISO\tCLK\tMOSI");
}


void ps2_settings(void) {
}


void ps2_help(void) {
    printf("ps2 mode\r\n");
    ui_help_mode_commands(ps2_commands, ps2_commands_count);
}

uint32_t ps2_get_speed(void) {
    return 10000000;
}