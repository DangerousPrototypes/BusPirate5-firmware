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
#include "pirate/psu.h"
#include "commands/global/p_pullups.h"
#include "pirate/mem.h"
#include "lib/bp_args/bp_cmd.h"

/*
MIT License - okhi - Open Keylogger Hardware Implant
---------------------------------------------------------------------------
Copyright (c) [2024] by David Reguera Garcia aka Dreg
https://github.com/therealdreg/pico-ps2-sniffer
https://github.com/therealdreg/okhi
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/structs/clocks.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "pico_ps2_sniffer.pio.h"
#include "commands/global/w_psu.h"

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

volatile static unsigned int write_index = 0;

static volatile uint8_t (*ringbuff)[32] = NULL;

volatile static unsigned int read_index = 0;
volatile static unsigned int dropped_packets = 0;
volatile static bool capture_enabled = false;

static unsigned char buffer_keys[32 * 10] = { 0 };
static unsigned int buffer_keys_index = 0;
static unsigned int total_pio_packets = 0;

volatile static uint kbd_h2d_sm;
volatile static uint offset_kbd_h2d;
volatile static uint kbd_sm;
volatile static uint offset_kbd;

#define BIO_DAT_GPIO BIO4
#define BIO_CLK_GPIO BIO5
#define BIO_AUX_H2D_JMP_GPIO BIO6
#define BIO_AUX_D2H_JMP_GPIO BIO7

#define DAT_GPIO (BIO_DAT_GPIO + 8)
#define CLK_GPIO (BIO_CLK_GPIO + 8)
#define AUX_H2D_JMP_GPIO (BIO_AUX_H2D_JMP_GPIO + 8)
#define AUX_D2H_JMP_GPIO  (BIO_AUX_D2H_JMP_GPIO + 8)

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

// the ISO key left of Z (102-key keyboards), \ and | like USB's non-US backslash
static const unsigned char MAKE_NON_US_BACKSLASH[] = {0x61};
static const unsigned char BREAK_NON_US_BACKSLASH[] = {0xF0, 0x61};

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
key_table_t keytable[] = {
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
    KEY_ENTRY(MAKE_NON_US_BACKSLASH, '\\'),
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
    KEY_ENTRY(BREAK_NON_US_BACKSLASH, '\\'),
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
};

// ---- Set 2 stream decoder ----------------------------------------------------------------------
//
// From okhi's PS/2 page decoder (RemoteKeyboardTracker in webps2/index.html), fed every byte in
// wire order, both directions. A table match over a byte buffer misread anything that broke a
// sequence: a reply in the middle, or the copy a keyboard sends after the host inhibited it
// mid-sequence, threw the buffer away and decoded the rest as another key. As state: E0 marks the
// next code extended, F0 a release, Pause (E1 and 7 more bytes, no break) is swallowed whole,
// keyboard replies never reach the table, the fake shifts E0 12 and E0 59 a keyboard wraps around
// some extended keys are not Shift, and Shift and CapsLock are tracked. A key gives its character
// on its first make, with the Shift of that moment: recording on the break typed "Hello" as
// "hello", and "a" for Shift down, A down, Shift up, A up (2026-11-03). A typematic repeat is the
// same make again and gives nothing. Host bytes are context: after Read ID (F2) the AB xx reply
// is not a key, after Resend (FE) the next device byte is a copy of the one before.

static struct
{
    bool ext;
    bool rel;
    uint8_t skip; // device bytes still to swallow: the rest of a Pause, the second ID byte
    bool shift_l;
    bool shift_r;
    bool caps;
    bool id_next;
    bool resend;
    uint8_t args; // host bytes still to come as arguments of the last command
    int last;     // (extended << 8) | code of the last key made, -1 for none
} dec;

static unsigned int device_bytes;
static unsigned int host_bytes;
static unsigned int dropped_total;

// The keytable entry with this key's make code, or its break code when released.
static const key_table_t *ps2_key_entry(uint8_t code, bool extended, bool released)
{
    unsigned char seq[3];
    size_t n = 0;

    // PrintScreen comes inside a fake shift (E0 12 E0 7C, E0 F0 7C E0 F0 12): here it is E0 7C
    if (extended && code == 0x7C)
    {
        const unsigned char *want = released ? BREAK_PRINT_SCREEN : MAKE_PRINT_SCREEN;
        for (int i = 0; i < (int)count_of(keytable); i++)
        {
            if (keytable[i].bytes == want)
            {
                return &keytable[i];
            }
        }
        return NULL;
    }

    if (extended)
    {
        seq[n++] = 0xE0;
    }
    if (released)
    {
        seq[n++] = 0xF0;
    }
    seq[n++] = code;

    for (int i = 0; i < (int)count_of(keytable); i++)
    {
        if (keytable[i].num_bytes == n && memcmp(keytable[i].bytes, seq, n) == 0)
        {
            return &keytable[i];
        }
    }
    return NULL;
}

// US layout, as the USB tracker: Shift picks the symbol, Shift and CapsLock cancel on letters.
static char ps2_char(uint8_t code, bool extended)
{
    static const uint8_t shifted_codes[] = { 0x16, 0x1E, 0x26, 0x25, 0x2E, 0x36, 0x3D, 0x3E, 0x46, 0x45, 0x0E,
                                             0x4E, 0x55, 0x5D, 0x54, 0x5B, 0x4C, 0x52, 0x41, 0x49, 0x4A, 0x61 };
    static const char shifted_chars[] = "!@#$%^&*()~_+|{}:\"<>?|";
    const key_table_t *entry = ps2_key_entry(code, extended, false);
    char c = entry ? entry->ascii : '\0';
    bool shift = dec.shift_l || dec.shift_r;

    // Enter, Tab and Backspace are named keys: the transcript keeps printable characters only
    if (c < ' ')
    {
        return '\0';
    }

    if (c >= 'a' && c <= 'z')
    {
        return (shift != dec.caps) ? (char)(c - 32) : c;
    }

    if (shift && !extended)
    {
        for (size_t i = 0; i < sizeof(shifted_codes); i++)
        {
            if (shifted_codes[i] == code)
            {
                return shifted_chars[i];
            }
        }
    }

    return c;
}

static const char *ps2_reply_name(uint8_t b)
{
    switch (b)
    {
        case 0xFA:
            return "ACK";
        case 0xAA:
            return "BAT OK";
        case 0xEE:
            return "ECHO";
        case 0xFE:
            return "RESEND";
        case 0xFC:
        case 0xFD:
            return "BAT FAILED";
        case 0x00:
        case 0xFF:
            return "KEY ERROR";
        default:
            return NULL;
    }
}

static const char *ps2_command_name(uint8_t b, uint8_t *args)
{
    *args = 0;
    switch (b)
    {
        case 0xED:
            *args = 1;
            return "SET LEDS";
        case 0xEE:
            return "ECHO";
        case 0xF0:
            *args = 1;
            return "SCAN CODE SET";
        case 0xF2:
            return "READ ID";
        case 0xF3:
            *args = 1;
            return "SET TYPEMATIC";
        case 0xF4:
            return "ENABLE";
        case 0xF5:
            return "DISABLE";
        case 0xF6:
            return "SET DEFAULT";
        case 0xFE:
            return "RESEND";
        case 0xFF:
            return "RESET";
        default:
            return NULL;
    }
}

static void ps2_decoder_reset(void)
{
    memset(&dec, 0, sizeof(dec));
    dec.last = -1;
}

static void ps2_tracker_add(char c)
{
    printf(" --> %c", c);
    buffer_keys[buffer_keys_index++ % (sizeof(buffer_keys) - 1)] = c;
    printf("\r\n**********\r\nps2 key tracker (limit 0x%02X bytes): %s\r\n**********",
           (unsigned int)(sizeof(buffer_keys) - 1), buffer_keys);
}

static void ps2_decode_host(uint8_t b)
{
    if (dec.args)
    {
        dec.args--;
        return;
    }

    uint8_t args;
    const char *name = ps2_command_name(b, &args);

    dec.args = args;
    if (b == 0xF2)
    {
        dec.id_next = true;
    }
    if (b == 0xFE)
    {
        dec.resend = true;
    }
    if (name)
    {
        printf("HOST %s\r\n-----------\r\n", name);
    }
}

static void ps2_decode_device(uint8_t b)
{
    if (dec.resend)
    {
        dec.resend = false;
        printf("RESENT\r\n-----------\r\n");
        return;
    }
    if (dec.skip)
    {
        dec.skip--;
        return;
    }
    if (dec.id_next && b == 0xAB)
    {
        dec.id_next = false;
        dec.skip = 1;
        printf("KEYBOARD ID\r\n-----------\r\n");
        return;
    }
    if (b == 0xE1)
    {
        dec.skip = 7;
        printf("MAKE_PAUSE_BREAK\r\n-----------\r\n");
        return;
    }
    if (b == 0xE0)
    {
        dec.ext = true;
        return;
    }
    if (b == 0xF0)
    {
        dec.rel = true;
        return;
    }

    const char *reply = ps2_reply_name(b);

    if (reply)
    {
        printf("%s\r\n-----------\r\n", reply);
        return;
    }

    bool e = dec.ext;
    bool r = dec.rel;

    dec.ext = false;
    dec.rel = false;

    if (e && (b == 0x12 || b == 0x59))
    {
        printf("FAKE SHIFT\r\n-----------\r\n");
        return;
    }

    const key_table_t *entry = ps2_key_entry(b, e, r);

    if (entry)
    {
        printf("%s", entry->name);
    }
    else
    {
        printf("%s_UNKNOWN_%s%02X", r ? "BREAK" : "MAKE", e ? "E0_" : "", b);
    }

    if (!e && b == 0x12)
    {
        dec.shift_l = !r;
    }
    else if (!e && b == 0x59)
    {
        dec.shift_r = !r;
    }

    int key = (e ? 0x100 : 0) | b;

    if (r)
    {
        if (dec.last == key)
        {
            dec.last = -1;
        }
    }
    else if (dec.last == key)
    {
        printf(" (repeat)");
    }
    else
    {
        dec.last = key;

        if (!e && b == 0x58)
        {
            dec.caps = !dec.caps;
        }

        char c = ps2_char(b, e);

        if (c)
        {
            ps2_tracker_add(c);
        }
    }

    printf("\r\n-----------\r\n");
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

static void ps2_ringbuff_put(uint8_t byte, bool device_to_host)
{
    if ((write_index - read_index) >= RING_BUFF_MAX_ENTRIES)
    {
        dropped_packets++;
        return;
    }
    const ps2_packet_t packet = { byte, us_to_ms(time_us_64()), device_to_host };
    memcpy((void *)&(ringbuff[write_index % RING_BUFF_MAX_ENTRIES][0]), &packet, sizeof(packet));
    write_index++;
}

static void ps2_drain_rx_fifo(PIO pio, uint sm, bool device_to_host)
{
    if (!capture_enabled)
    {
        return;
    }
    while (!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        uint8_t byte = *((io_rw_8 *)&pio->rxf[sm] + 3);
        ps2_ringbuff_put(byte, device_to_host);
    }
}

void pio0_irq(void)
{
    ps2_drain_rx_fifo(pio0, kbd_sm, true);

    if (pio0_hw->irq & 1)
    {
        pio0_hw->irq = 1;
        stop_device_to_host_sm();
    }
    else if (pio0_hw->irq & 2)
    {
        pio0_hw->irq = 2;
        start_device_to_host_sm();
        restart_host_to_device_sm();
    }
}

void pio1_irq(void)
{
    ps2_drain_rx_fifo(pio1, kbd_h2d_sm, false);

    if (pio1_hw->irq & 1)
    {
        pio1_hw->irq = 1;
        start_device_to_host_sm();
        restart_host_to_device_sm();
    }
    else if (pio1_hw->irq & 2)
    {
        pio1_hw->irq = 2;
    }
}

static void ps2_capture_reset(void)
{
    write_index = 0;
    read_index = 0;
    dropped_packets = 0;
    dropped_total = 0;
    total_pio_packets = 0;
    device_bytes = 0;
    host_bytes = 0;
    buffer_keys_index = 0;
    memset((void *)buffer_keys, 0, sizeof(buffer_keys));
    ps2_decoder_reset();
}

static void ps2_process_packets(void)
{
    while (read_index != write_index)
    {
        ps2_packet_t packet;
        memcpy(&packet, (const void *)&(ringbuff[read_index % RING_BUFF_MAX_ENTRIES][0]), sizeof(packet));
        read_index++;

        printf("[0x%04X] t:0x%08X %c:0x%02X\r\n", total_pio_packets, packet.timestamp,
               packet.device_to_host ? 'D' : 'H', packet.byte);
        total_pio_packets++;

        if (packet.device_to_host)
        {
            device_bytes++;
            ps2_decode_device(packet.byte);
        }
        else
        {
            host_bytes++;
            ps2_decode_host(packet.byte);
        }
    }

    if (dropped_packets)
    {
        unsigned int dropped = dropped_packets;
        dropped_packets = 0;
        dropped_total += dropped;
        printf("\r\n!! ring buffer full, %u packet(s) dropped !!\r\n", dropped);
    }
}

// ---- PIO ---------------------------------------------------------------------------------------
//
// host_to_device and idle_signal go in the mode's PIO block, inhibited_signal and device_to_host
// in pio0 on state machines 2 and 3: the logic analyzer and, on the Bus Pirate 5, the RGB LEDs run
// on pio0 SM 0 and 1 without claiming them. Only these four programs are added and removed: clearing
// both blocks also wiped the LED program. 'q' now stops them and their interrupts too; they used to
// run on until the mode was left.

static bool ps2_pio_loaded = false;
static uint kbd_idle_sm;
static uint kbd_inh_sm;
static uint offset_idle;
static uint offset_inh;

static void ps2_pio_unload(void)
{
    capture_enabled = false;
    // only our own handlers: these interrupts may belong to the logic analyzer
    if (irq_get_exclusive_handler(PIO0_IRQ_0) == pio0_irq)
    {
        irq_set_enabled(PIO0_IRQ_0, false);
        irq_remove_handler(PIO0_IRQ_0, pio0_irq);
    }
    if (irq_get_exclusive_handler(PIO1_IRQ_0) == pio1_irq)
    {
        irq_set_enabled(PIO1_IRQ_0, false);
        irq_remove_handler(PIO1_IRQ_0, pio1_irq);
    }

    if (!ps2_pio_loaded)
    {
        return;
    }
    ps2_pio_loaded = false;

    pio_set_irq0_source_mask_enabled(pio0, 0x0F00, false);
    pio_set_irq0_source_enabled(pio0, (enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + kbd_sm), false);
    pio_set_irq0_source_mask_enabled(pio1, 0x0F00, false);
    pio_set_irq0_source_enabled(pio1, (enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + kbd_h2d_sm), false);

    pio_sm_set_enabled(pio0, kbd_inh_sm, false);
    pio_sm_set_enabled(pio0, kbd_sm, false);
    pio_sm_unclaim(pio0, kbd_inh_sm);
    pio_sm_unclaim(pio0, kbd_sm);
    pio_remove_program(pio0, &inhibited_signal_program, offset_inh);
    pio_remove_program(pio0, &device_to_host_program, offset_kbd);
    pio0_hw->irq = 0x3; // the inhibit detector's two flags

    pio_sm_set_enabled(pio1, kbd_h2d_sm, false);
    pio_sm_set_enabled(pio1, kbd_idle_sm, false);
    pio_sm_unclaim(pio1, kbd_h2d_sm);
    pio_sm_unclaim(pio1, kbd_idle_sm);
    pio_remove_program(pio1, &host_to_device_program, offset_kbd_h2d);
    pio_remove_program(pio1, &idle_signal_program, offset_idle);
    pio1_hw->irq = 0x1; // the idle detector's flag

    bio_input(BIO_AUX_D2H_JMP_GPIO);
    bio_input(BIO_AUX_H2D_JMP_GPIO);
}

// Load the four programs and start them. False, with the reason printed, if a PIO is busy.
static bool ps2_pio_load(void)
{
    irq_handler_t h0 = irq_get_exclusive_handler(PIO0_IRQ_0);
    irq_handler_t h1 = irq_get_exclusive_handler(PIO1_IRQ_0);

    if ((h0 && h0 != pio0_irq) || (h1 && h1 != pio1_irq))
    {
        printf("the PIO interrupts are in use (logic analyzer?), not sniffing\r\n");
        return false;
    }

    // The mode's PIO belongs to the active mode: clear whatever an earlier one left in it.
    for (int sm = 0; sm < 4; sm++)
    {
        if (pio_sm_is_claimed(pio1, sm))
        {
            pio_sm_set_enabled(pio1, sm, false);
            pio_sm_unclaim(pio1, sm);
        }
    }
    pio_clear_instruction_memory(pio1);
#if PICO_PIO_USE_GPIO_BASE
    pio_set_gpio_base(pio1, 0);
#endif

    if (pio_sm_is_claimed(pio0, 2) || pio_sm_is_claimed(pio0, 3) ||
        !pio_can_add_program(pio0, &inhibited_signal_program))
    {
        printf("pio0 has no free state machine or no room, not sniffing\r\n");
        return false;
    }
    offset_inh = pio_add_program(pio0, &inhibited_signal_program);
    if (!pio_can_add_program(pio0, &device_to_host_program))
    {
        pio_remove_program(pio0, &inhibited_signal_program, offset_inh);
        printf("pio0 has no room, not sniffing\r\n");
        return false;
    }
    offset_kbd = pio_add_program(pio0, &device_to_host_program);
    kbd_inh_sm = 2;
    kbd_sm = 3;
    pio_sm_claim(pio0, kbd_inh_sm);
    pio_sm_claim(pio0, kbd_sm);

    kbd_h2d_sm = pio_claim_unused_sm(pio1, true);
    offset_kbd_h2d = pio_add_program(pio1, &host_to_device_program);
    kbd_idle_sm = pio_claim_unused_sm(pio1, true);
    offset_idle = pio_add_program(pio1, &idle_signal_program);
    ps2_pio_loaded = true;

    bio_input(BIO_DAT_GPIO);
    bio_input(BIO_CLK_GPIO);
    bio_output(BIO_AUX_D2H_JMP_GPIO);
    bio_output(BIO_AUX_H2D_JMP_GPIO);
    bio_put(BIO_AUX_D2H_JMP_GPIO, false);
    bio_put(BIO_AUX_H2D_JMP_GPIO, true);

    pio_sm_set_consecutive_pindirs(pio1, kbd_h2d_sm, DAT_GPIO, 2, false);
    // host_to_device's own wrap: device_to_host's left its last instruction (the ACK clock) out
    pio_sm_config c_kbd_h2d = host_to_device_program_get_default_config(offset_kbd_h2d);
    sm_config_set_in_pins(&c_kbd_h2d, DAT_GPIO);
    sm_config_set_in_shift(&c_kbd_h2d, true, false, 0);
    sm_config_set_jmp_pin(&c_kbd_h2d, CLK_GPIO);
    sm_config_set_fifo_join(&c_kbd_h2d, PIO_FIFO_JOIN_RX);
    float div_kbd_h2d = (float)clock_get_hz(clk_sys) / (8 * 16700);
    sm_config_set_clkdiv(&c_kbd_h2d, div_kbd_h2d);
    pio_sm_init(pio1, kbd_h2d_sm, offset_kbd_h2d, &c_kbd_h2d);
    pio_sm_set_enabled(pio1, kbd_h2d_sm, false);
    pio_sm_clear_fifos(pio1, kbd_h2d_sm);
    pio_sm_restart(pio1, kbd_h2d_sm);
    pio_sm_clkdiv_restart(pio1, kbd_h2d_sm);
    pio_sm_set_enabled(pio1, kbd_h2d_sm, true);
    pio_sm_exec(pio1, kbd_h2d_sm, pio_encode_jmp(offset_kbd_h2d));

    pio_sm_set_consecutive_pindirs(pio1, kbd_idle_sm, DAT_GPIO, 2, false);
    pio_sm_config c_idle = idle_signal_program_get_default_config(offset_idle);
    sm_config_set_in_pins(&c_idle, DAT_GPIO);
    sm_config_set_fifo_join(&c_idle, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&c_idle, CLK_GPIO);
    float div_idle = clock_get_hz(clk_sys) / 1000000.0;
    sm_config_set_clkdiv(&c_idle, div_idle);
    pio_set_irq0_source_mask_enabled(pio1, 0x0F00, true);
    irq_set_exclusive_handler(PIO1_IRQ_0, pio1_irq);
    irq_set_enabled(PIO1_IRQ_0, true);
    pio_sm_init(pio1, kbd_idle_sm, offset_idle, &c_idle);
    pio_sm_set_enabled(pio1, kbd_idle_sm, false);
    pio_sm_clear_fifos(pio1, kbd_idle_sm);
    pio_sm_restart(pio1, kbd_idle_sm);
    pio_sm_clkdiv_restart(pio1, kbd_idle_sm);
    pio_sm_set_enabled(pio1, kbd_idle_sm, true);
    pio_sm_exec(pio1, kbd_idle_sm, pio_encode_jmp(offset_idle));

    pio_sm_set_consecutive_pindirs(pio0, kbd_inh_sm, DAT_GPIO, 2, false);
    pio_sm_config c_inh = inhibited_signal_program_get_default_config(offset_inh);
    sm_config_set_in_pins(&c_inh, DAT_GPIO);
    sm_config_set_fifo_join(&c_inh, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&c_inh, CLK_GPIO);
    float div_inh = clock_get_hz(clk_sys) / 1000000.0;
    sm_config_set_clkdiv(&c_inh, div_inh);
    pio_set_irq0_source_mask_enabled(pio0, 0x0F00, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_sm_init(pio0, kbd_inh_sm, offset_inh, &c_inh);
    pio_sm_set_enabled(pio0, kbd_inh_sm, false);
    pio_sm_clear_fifos(pio0, kbd_inh_sm);
    pio_sm_restart(pio0, kbd_inh_sm);
    pio_sm_clkdiv_restart(pio0, kbd_inh_sm);
    pio_sm_set_enabled(pio0, kbd_inh_sm, true);
    pio_sm_exec(pio0, kbd_inh_sm, pio_encode_jmp(offset_inh));

    pio_sm_set_consecutive_pindirs(pio0, kbd_sm, DAT_GPIO, 3, false);
    pio_sm_config c_kbd = device_to_host_program_get_default_config(offset_kbd);
    sm_config_set_in_pins(&c_kbd, DAT_GPIO);
    sm_config_set_in_shift(&c_kbd, true, false, 0);
    sm_config_set_jmp_pin(&c_kbd, AUX_D2H_JMP_GPIO);
    sm_config_set_fifo_join(&c_kbd, PIO_FIFO_JOIN_RX);
    float div_kbd = (float)clock_get_hz(clk_sys) / (8 * 16700);
    sm_config_set_clkdiv(&c_kbd, div_kbd);
    pio_sm_init(pio0, kbd_sm, offset_kbd, &c_kbd);
    pio_sm_set_enabled(pio0, kbd_sm, false);
    pio_sm_clear_fifos(pio0, kbd_sm);
    pio_sm_restart(pio0, kbd_sm);
    pio_sm_clkdiv_restart(pio0, kbd_sm);
    pio_sm_set_enabled(pio0, kbd_sm, true);
    pio_sm_exec(pio0, kbd_sm, pio_encode_jmp(offset_kbd));

    start_device_to_host_sm();
    restart_host_to_device_sm();

    ps2_capture_reset();
    pio_sm_clear_fifos(pio0, kbd_sm);
    pio_sm_clear_fifos(pio1, kbd_h2d_sm);
    capture_enabled = true;
    pio_set_irq0_source_enabled(pio0, (enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + kbd_sm), true);
    pio_set_irq0_source_enabled(pio1, (enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + kbd_h2d_sm), true);
    return true;
}

// ---- PS/2 host ---------------------------------------------------------------------------------
//
// Bus syntax makes the Bus Pirate the keyboard's only host: a PS/2 keyboard on the plank and
// nothing on the other PS/2 connector. A value is a command byte, shown with the keyboard's
// answer, and 'r' reads one byte. Between transfers the Bus Pirate holds the keyboard with CLK low,
// as a PC does when it can't take a byte, so the keyboard keeps what it wants to send until the
// next 'r': nothing is lost while the next command runs or the user types. '[' holds the keyboard
// too, ']' lets it go. The IO buffers are push-pull: a line is pulled low with the buffer driving
// 0 and released with the buffer as an input. The pull-ups come from the keyboard or from 'P'.

#define PS2H_INHIBIT_US 120  // CLK low before a request to send, at least 100 us
#define PS2H_START_US 15000  // the keyboard starts clocking a request within 15 ms
#define PS2H_EDGE_US 2000    // a whole frame takes at most 2 ms, so no two edges are further apart
#define PS2H_ANSWER_US 25000 // a command's answer comes within 20 ms
#define PS2H_READ_US 1000000 // 'r' waits up to 1 s, enough for the BAT after a reset

static const char ps2h_no_data[] = "no data from the keyboard";
static const char ps2h_lines_low[] = "CLK or DAT low: no keyboard, or no pull-ups";
static const char ps2h_bad_frame[] = "framing error";
static const char ps2h_bad_parity[] = "parity error";

static struct
{
    bool held;  // CLK held low by the Bus Pirate
    bool ext;   // E0 read: the next code is an extended key
    bool rel;   // F0 read: the next code is a break
    uint8_t id; // keyboard ID bytes still to come after READ ID
} host;

static void ps2h_low(uint8_t bio)
{
    bio_put(bio, false);
    bio_output(bio);
}

static void ps2h_release(uint8_t bio)
{
    bio_input(bio);
}

static bool ps2h_wait(uint8_t bio, bool level, uint32_t timeout_us)
{
    uint32_t t0 = time_us_32();

    while (bio_get(bio) != level)
    {
        if (time_us_32() - t0 > timeout_us)
        {
            return false;
        }
    }
    return true;
}

// between transfers: DAT free, CLK low
static void ps2h_hold(void)
{
    ps2h_release(BIO_DAT_GPIO);
    ps2h_low(BIO_CLK_GPIO);
    host.held = true;
}

static void ps2h_free(void)
{
    memset(&host, 0, sizeof(host));
    ps2h_release(BIO_DAT_GPIO);
    ps2h_release(BIO_CLK_GPIO);
}

// One host to device frame: CLK low, request to send (DAT low, CLK released), then the 8 data
// bits LSB first and odd parity, each set while the keyboard holds CLK low, the stop bit, and the
// keyboard's ACK bit. Returns NULL or what went wrong.
static const char *ps2h_send(uint8_t b)
{
    bool parity = !(__builtin_popcount(b) & 1);

    if (!bio_get(BIO_DAT_GPIO) || (!host.held && !bio_get(BIO_CLK_GPIO)))
    {
        return ps2h_lines_low;
    }

    ps2h_low(BIO_CLK_GPIO);
    busy_wait_us_32(PS2H_INHIBIT_US);
    ps2h_low(BIO_DAT_GPIO);
    ps2h_release(BIO_CLK_GPIO);

    // CLK has to rise first: right after the release it still reads the low we drove
    if (!ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US))
    {
        return "CLK stays low: no pull-ups";
    }
    if (!ps2h_wait(BIO_CLK_GPIO, false, PS2H_START_US))
    {
        return "no clock from the keyboard";
    }

    for (int i = 0; i < 9; i++)
    {
        if (i < 8 ? (b >> i) & 1 : parity)
        {
            ps2h_release(BIO_DAT_GPIO);
        }
        else
        {
            ps2h_low(BIO_DAT_GPIO);
        }
        if (!ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US) || !ps2h_wait(BIO_CLK_GPIO, false, PS2H_EDGE_US))
        {
            return "the keyboard stopped clocking";
        }
    }

    ps2h_release(BIO_DAT_GPIO); // the stop bit, read by the keyboard on the next rising edge

    // After a 0 parity bit DAT still reads the low we drove for a moment: the ACK can only come
    // after that rising edge, or the low is taken for the ACK and the answer is read a bit late.
    if (!ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US))
    {
        return "the keyboard stopped clocking";
    }
    if (!ps2h_wait(BIO_DAT_GPIO, false, PS2H_EDGE_US))
    {
        return "no ACK bit from the keyboard";
    }
    if (!ps2h_wait(BIO_CLK_GPIO, false, PS2H_EDGE_US) || !ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US) ||
        !ps2h_wait(BIO_DAT_GPIO, true, PS2H_EDGE_US))
    {
        return "the keyboard stopped clocking";
    }
    return NULL;
}

// One device to host frame: the start bit, 8 data bits LSB first, odd parity and the stop bit,
// each read while the keyboard holds CLK low. Returns NULL, ps2h_no_data, or what went wrong;
// after ps2h_bad_frame and ps2h_bad_parity the byte is in *out anyway.
static const char *ps2h_recv(uint8_t *out, uint32_t timeout_us)
{
    uint32_t bits = 0;

    ps2h_release(BIO_CLK_GPIO);

    // a keyboard on hold starts sending only after CLK has been high for 50 us
    if (!ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US))
    {
        return "CLK stays low: no pull-ups";
    }
    if (!ps2h_wait(BIO_CLK_GPIO, false, timeout_us))
    {
        return ps2h_no_data;
    }

    for (int i = 0; i < 11; i++)
    {
        if (i && (!ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US) || !ps2h_wait(BIO_CLK_GPIO, false, PS2H_EDGE_US)))
        {
            return "the keyboard stopped clocking";
        }
        bits |= (uint32_t)bio_get(BIO_DAT_GPIO) << i;
    }
    ps2h_wait(BIO_CLK_GPIO, true, PS2H_EDGE_US);

    *out = (uint8_t)(bits >> 1);

    if ((bits & 1) || !(bits & (1u << 10)))
    {
        return ps2h_bad_frame;
    }
    if (!(__builtin_popcount((bits >> 1) & 0x1FF) & 1))
    {
        return ps2h_bad_parity;
    }
    return NULL;
}

static bool ps2h_got_byte(const char *err)
{
    return !err || err == ps2h_bad_frame || err == ps2h_bad_parity;
}

// the name of a byte from the keyboard: a reply, the keyboard ID, or the key it completes
static const char *ps2h_name(uint8_t b)
{
    if (host.id)
    {
        return (host.id-- == 2) ? "KEYBOARD ID" : NULL;
    }

    const char *reply = ps2_reply_name(b);

    if (reply)
    {
        host.ext = false;
        host.rel = false;
        return reply;
    }
    if (b == 0xE0)
    {
        host.ext = true;
        return NULL;
    }
    if (b == 0xF0)
    {
        host.rel = true;
        return NULL;
    }

    const key_table_t *entry = ps2_key_entry(b, host.ext, host.rel);

    host.ext = false;
    host.rel = false;
    return entry ? entry->name : NULL;
}

void ps2_host_start(struct _bytecode *result, struct _bytecode *next)
{
    (void)next;
    ps2h_hold();
    result->data_message = "keyboard on hold (CLK low)";
}

void ps2_host_stop(struct _bytecode *result, struct _bytecode *next)
{
    (void)next;
    ps2h_free();
    result->data_message = "keyboard released";
}

void ps2_host_write(struct _bytecode *result, struct _bytecode *next)
{
    (void)next;
    uint8_t b = (uint8_t)result->out_data;
    const char *err = ps2h_send(b);

    host.ext = false;
    host.rel = false;
    host.id = (b == 0xF2) ? 2 : 0;

    if (err)
    {
        // Lines low already: nothing was driven, so nothing changes and 'v' shows what pulls them
        // down. A request the keyboard did not take: both lines are let go.
        if (err != ps2h_lines_low)
        {
            ps2h_free();
        }
        result->error = SERR_ERROR;
        result->error_message = err;
        return;
    }

    uint8_t answer = 0;

    err = ps2h_recv(&answer, PS2H_ANSWER_US);
    ps2h_hold();
    if (ps2h_got_byte(err))
    {
        result->read_with_write = true;
        result->in_data = answer;
        result->data_message = ps2_reply_name(answer);
    }
    if (err)
    {
        result->error = SERR_WARN;
        result->error_message = (err == ps2h_no_data) ? "no answer" : err;
    }
}

void ps2_host_read(struct _bytecode *result, struct _bytecode *next)
{
    (void)next;
    uint8_t b = 0;
    const char *err = ps2h_recv(&b, PS2H_READ_US);

    ps2h_hold();
    if (!ps2h_got_byte(err))
    {
        result->error = SERR_ERROR;
        result->error_message = err;
        return;
    }
    result->in_data = b;
    result->data_message = ps2h_name(b);
    if (err)
    {
        result->error = SERR_WARN;
        result->error_message = err;
    }
}

void ps2_handler(struct command_result* res) {
    (void)res;

    ps2h_free(); // a keyboard held by '[' would never talk to the PC

    if (NULL == ringbuff)
    {
        printf("Buffer not allocated\r\n");
        return;
    }

    // psucmd_enable() also labels the VOUT pin, which psu_enable() alone left at OFF with 5 V on it
    uint32_t result = PSU_OK;

    if (!psu_status.enabled || fabsf(psu_status.voltage_requested_float - 5.0f) > 0.05f)
    {
        result = psucmd_enable(5.0f, 400.0f, false, 100);
    }

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

    // No pull-ups: the host already pulls DAT and CLK up. On the Bus Pirate 5 and 6 one switch pulls
    // all eight pins, so they also pulled D+ and D- of the plank's USB port to VOUT (3.0 V on both)
    // and the PC showed an unknown device there (VID_0000&PID_0002, 2026-11-03).

    char cmd;
    while (rx_fifo_try_get(&cmd))
    {
        busy_wait_ms(10);
    }

    if (!ps2_pio_load())
    {
        ps2_pio_unload();
        return;
    }

    printf("\r\nPress 'q' to exit\r\n\r\nSniffing...\r\n\r\n");

    while (1)
    {
            ps2_process_packets();

            if (rx_fifo_try_get(&cmd))
            {
                if (cmd == 'q' || cmd == 'Q')
                {
                    ps2_pio_unload();
                    ps2_process_packets();
                    printf("\r\n\r\nkeys captured: %s\r\n", buffer_keys);
                    printf("device to host bytes: %u\r\n", device_bytes);
                    printf("host to device bytes: %u\r\n", host_bytes);
                    printf("bytes dropped: %u\r\n", dropped_total);
                    printf("\r\nexit...\r\n");
                    return;
                }
            }
    }
}

const bp_command_def_t ps2_sniff_def = {
    .name        = "sniff",
    .description = T_PS2_DESCRIPTION,
};

const struct _mode_command_struct ps2_commands[] = {
    {   .func=&ps2_handler,
        .def=&ps2_sniff_def,
        .supress_fala_capture=true
    },

};
const uint32_t ps2_commands_count = count_of(ps2_commands);

static const char pin_labels[][5] = { "TRIG", "D+", "D-", "RESV", "DAT", "CLK", "RESV", "RESV" };

uint32_t ps2_setup(void) {

    return 1;
}

uint32_t ps2_setup_exc(void) {
    bio_init();
    ps2h_free();
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
    ps2h_free();
    ps2_pio_unload();

    if (ringbuff)
    {
        mem_free((uint8_t*)ringbuff);
        ringbuff = NULL;
    }

    system_bio_update_purpose_and_label(false, BIO0, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
    bio_init();

    psucmd_disable();
}

bool ps2_preflight_sanity_check(void){
    ui_help_sanity_check(true, 0x00);
    return true;
}

void ps2_pins(void){
    printf("TRST\tSRST\tCS\tMISO\tCLK\tMOSI");
}

void ps2_settings(void) {
}

void ps2_help(void) {
    printf("ps2 mode\r\n");
    printf("Bus syntax, the Bus Pirate as the only host of a PS/2 keyboard:\r\n"
           "  0xED 0x07  send a command, the keyboard's answer in ()\r\n"
           "  r          read a byte from the keyboard\r\n"
           "  [ ]        hold the keyboard (CLK low) / let it go\r\n");
    ui_help_mode_commands(ps2_commands, ps2_commands_count);
}

uint32_t ps2_get_speed(void) {
    return 10000000;
}
