/**
 * @file ui_term.c
 * @brief Terminal control and VT100 support implementation.
 * @details Implements terminal detection and control:
 *          - VT100 terminal detection via cursor position query
 *          - RGB and 256-color ANSI color support
 *          - Command-line editing (cursor movement, history)
 *          - Progress bar display
 *          
 *          Color system:
 *          - Detects terminal capabilities on startup
 *          - Falls back to no-color mode if terminal unsupported
 *          - Supports both full RGB and 256-color ANSI modes
 *          - Optional ANSI_COLOR_256 library integration (LGPL3)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "usb_tx.h"
#include "usb_rx.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_statusbar.h"
#ifdef ANSI_COLOR_256
#include "ansi_colours.h"
/**
 * The library ansi_colours is licensed under LGPL3 and copyrighted by Michał Nazarewicz <mina86@mina86.com> 2018.
 * License compliance is explained at docs/licenses.md
 */
#endif

// printf("你好小狗");
//  ESC [ ? 3 h       132 column mode
//   ESC [ ? 3 l       80 column mode

int ui_term_get_vt100_query(const char* query, char end_of_line, char* result, uint8_t max_length);

int ui_term_get_vt100_query(const char* query, char end_of_line, char* result, uint8_t max_length) {
    uint32_t timeout = 10000;
    int i = 0;
    char c;

    // Clear any stuff from buffer
    while (rx_fifo_try_get(&c))
        ;

    // Send query
    printf("%s", query);

    // Receive response
    while (timeout > 0) {
        busy_wait_us(100);
        if (rx_fifo_try_get(&c)) {
            result[i] = c;
            if (result[i] == end_of_line) {
                return i;
            }
            i++;
            if (i == max_length) {
                return -2;
            }
        }
        timeout--;
    }

    return -1;
}

bool ui_term_detect_vt100(uint32_t* row, uint32_t* col) {
    uint8_t cp[20];
    int p;
    uint32_t r = 0;
    uint32_t c = 0;
    uint8_t stage = 0;

    // Position cursor at extreme corner and get the actual postion
    p = ui_term_get_vt100_query("\0337\033[999;999H\033[6n\0338", 'R', (char*)cp, 20);

    // no reply, no terminal connected or doesn't support VT100
    if (p < 0) {
        return false;
    }
    // Extract cursor position from response
    for (int i = 0; i < p; i++) {
        switch (stage) {
            case 0:
                if (cp[i] == '[') {
                    stage = 1;
                }
                break;
            case 1: // Rows
                if (cp[i] == ';') {
                    stage = 2;
                    break;
                }
                r *= 10;
                r += cp[i] - 0x30;
                break;
            case 2: // Columns
                if (cp[i] == 'R') {
                    stage = 3;
                    break;
                }
                c *= 10;
                c += cp[i] - 0x30;
                break;
            default:
                break;
        }
    }

    // printf("Terminal: %d rows, %d cols\r\n", r, c);
    if (r == 0 || c == 0) {
        // non-detection fallback
        return false;
    }
    *row = r;
    *col = c;
    return true;
}

bool ui_term_detect(void) {
    uint32_t row = 0;
    uint32_t col = 0;
    if (!ui_term_detect_vt100(&row, &col)) {
        system_config.terminal_ansi_statusbar = false;
        system_config.terminal_ansi_color = UI_TERM_NO_COLOR;
        printf("VT100 terminal not detected!\r\nDefaulting to ASCII mode.\r\n");
        return false;
    }

    if (system_config.terminal_ansi_rows != row || system_config.terminal_ansi_columns != col) {
        printf("Screen Resolution changed\r\n");
    }

    system_config.terminal_ansi_rows = row;
    system_config.terminal_ansi_columns = col;
    return true;
}

void ui_term_init(void) {
    if (system_config.terminal_ansi_color) {
        printf("\033[?3l"); // 80 columns
        printf("\033]0;%s\033\\", BP_HARDWARE_VERSION);
        // reset all styling
        printf("\033[0m");
        // set cursor type
        // printf("\e[3 q");
        // clear screen
        printf("\033[2J");
    }
}

/*
\033  escape
[0m   reset/normal
[38;2;<r>;<g>;<b>m  set rgb text color
[48;2;<r>;<g>;<b>m  set rgb background color
[38;2;<r>;<g>;<b>;48;2;<r>;<g>;<b>m set text and background color
\033P$qm\033\\  query current settings, can be used to test true color support on SOME terminals... doesn't seem to be
widly used

*/

void ui_term_color_text(uint32_t rgb) {
    switch (system_config.terminal_ansi_color) {
        case UI_TERM_FULL_COLOR:
            printf("\033[38;2;%d;%d;%dm", (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)(rgb));
            break;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            printf("\033[38;5;%hhdm", ansi256_from_rgb(rgb));
            break;
#endif
        case UI_TERM_NO_COLOR:
        default:
            break;
    }
}

void ui_term_color_background(uint32_t rgb) {
    switch (system_config.terminal_ansi_color) {
        case UI_TERM_FULL_COLOR:
            printf("\033[48;2;%d;%d;%dm", (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)(rgb));
            break;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            printf("\033[48;5;%hhdm", ansi256_from_rgb(rgb));
            break;
#endif
        case UI_TERM_NO_COLOR:
        default:
            break;
    }
}

uint32_t ui_term_color_text_background(uint32_t rgb_text, uint32_t rgb_background) {
    uint32_t count = 0;

    switch (system_config.terminal_ansi_color) {
        case UI_TERM_FULL_COLOR:
            count = printf("\033[38;2;%d;%d;%d;48;2;%d;%d;%dm",
                           (uint8_t)(rgb_text >> 16),
                           (uint8_t)(rgb_text >> 8),
                           (uint8_t)(rgb_text),
                           (uint8_t)(rgb_background >> 16),
                           (uint8_t)(rgb_background >> 8),
                           (uint8_t)(rgb_background));
            return count;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            count = printf("\033[38;5;%hhd;48;5;%hhdm", ansi256_from_rgb(rgb_text), ansi256_from_rgb(rgb_background));
            return count;
#endif
        case UI_TERM_NO_COLOR:
        default:
            break;
    }
    return 0;
}

uint32_t ui_term_color_text_background_buf(char* buf, size_t buffLen, uint32_t rgb_text, uint32_t rgb_background) {
    uint32_t count = 0;
    switch (system_config.terminal_ansi_color) {
        case UI_TERM_FULL_COLOR:
            count = snprintf(buf,
                             buffLen,
                             "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm",
                             (uint8_t)(rgb_text >> 16),
                             (uint8_t)(rgb_text >> 8),
                             (uint8_t)(rgb_text),
                             (uint8_t)(rgb_background >> 16),
                             (uint8_t)(rgb_background >> 8),
                             (uint8_t)(rgb_background));
            return count;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            count = snprintf(buf,
                             buffLen,
                             "\033[38;5;%hhd;48;5;%hhdm",
                             ansi256_from_rgb(rgb_text),
                             ansi256_from_rgb(rgb_background));
            return count;
#endif
        case UI_TERM_NO_COLOR:
        default:
            break;
    }

    return 0;
}

#define UI_TERM_FULL_COLOR_CONCAT_TEXT(color) ("\033[38;2;" color "m")
#define UI_TERM_FULL_COLOR_CONCAT_BACKGROUND(color) ("\033[48;2;" color "m")
#define UI_TERM_256_COLOR_CONCAT_TEXT(color) ("\033[38;5;" color "m")
#define UI_TERM_256_COLOR_CONCAT_BACKGROUND(color) ("\033[48;5;" color "m")

char* ui_term_color_reset(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
#endif
        case UI_TERM_FULL_COLOR:
            return "\033[0m";
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_prompt(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_PROMPT_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_PROMPT_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_info(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_INFO_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_INFO_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_notice(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_NOTICE_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_NOTICE_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_warning(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_WARNING_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_WARNING_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_error(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_ERROR_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_ERROR_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_num_float(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_NUM_FLOAT_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_NUM_FLOAT_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_grey(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT(BP_COLOR_256_GREY_TEXT);
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT(BP_COLOR_GREY_TEXT);
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_color_pacman(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            return UI_TERM_256_COLOR_CONCAT_TEXT("3");
#endif
        case UI_TERM_FULL_COLOR:
            return UI_TERM_FULL_COLOR_CONCAT_TEXT("255;238;00");
        case UI_TERM_NO_COLOR:
        default:
            return "";
    }
}

char* ui_term_cursor_hide(void) {
    return system_config.terminal_ansi_color ? "\033[?25l" : "";
}
char* ui_term_cursor_show(void) {
    return !system_config.terminal_hide_cursor && system_config.terminal_ansi_color ? "\033[?25h" : "";
}

void ui_term_progress_bar_draw(ui_term_progress_bar_t* pb) {
    system_config.terminal_hide_cursor = true;
    busy_wait_ms(1);
    printf("%s\r%s[%s", ui_term_cursor_hide(), ui_term_color_prompt(), ui_term_color_reset());
    for (int8_t i = 0; i < 20; i++) {
        if (i % 2) {
            printf(" ");
        } else {
            printf("o");
        }
    }
    printf("%s]\r\033[1C", ui_term_color_prompt());
    pb->indicator_state = 1;
    pb->previous_pct = 0;
    pb->progress_cnt = 0;
}

void ui_term_progress_bar_update(uint32_t current, uint32_t total, ui_term_progress_bar_t* pb) {
    uint32_t pct = ((current) * 20) / (total);
    uint32_t previous_pct = pct - pb->previous_pct;

    system_config.terminal_ansi_statusbar_pause = true;
    if ((previous_pct) > 0) {
        for (uint32_t i = 0; i < (previous_pct); i++) // advance this many positions
        {
            printf("%s-", ui_term_color_prompt());
        }
    }

    if ((pb->progress_cnt > 600) || ((previous_pct) > 0)) // gone 5 loops without an advance
    {
        printf("%s%c\033[1D", ui_term_color_pacman(), (pb->indicator_state) ? 'C' : 'c'); // C and reset the cursor
        if (pb->progress_cnt > 600) {
            pb->progress_cnt = 0;
        }
        pb->indicator_state = !pb->indicator_state;
        pb->previous_pct = pct;
    }
    system_config.terminal_ansi_statusbar_pause = false;
    pb->progress_cnt++;
}

void ui_term_progress_bar_cleanup(ui_term_progress_bar_t* pb) {
    system_config.terminal_hide_cursor = false;
    printf("%s%s\r\n", ui_term_color_reset(), ui_term_cursor_show());
}

// wait for a specific char from the cmdline (or any if NULL)
// return the char sent in case we want to do more complex control
char ui_term_cmdln_wait_char(char c) {
    char r = '\0';
    while (!system_config.error) {
        if (rx_fifo_try_get(&r) && ((r == c) || c == '\0')) {
            return r;
        }
        busy_wait_ms(1);
    }
}