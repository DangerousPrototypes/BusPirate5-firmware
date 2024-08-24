#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "usb_tx.h"
#include "usb_rx.h"
#include "ui/ui_cmdln.h"
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

// Determine the terminal rows and columns
void ui_term_detect(void) {
    uint8_t cp[20];
    int p;
    uint32_t row = 0;
    uint32_t col = 0;
    uint8_t stage = 0;

    // Position cursor at extreme corner and get the actual postion
    p = ui_term_get_vt100_query("\e7\e[999;999H\e[6n\e8", 'R', cp, 20);

    // no reply, no terminal connected or doesn't support VT100
    if (p < 0) {
        system_config.terminal_ansi_statusbar = 0;
        system_config.terminal_ansi_color = UI_TERM_NO_COLOR;
        return;
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
                row *= 10;
                row += cp[i] - 0x30;
                break;
            case 2: // Columns
                if (cp[i] == 'R') {
                    stage = 3;
                    break;
                }
                col *= 10;
                col += cp[i] - 0x30;
                break;
            default:
                break;
        }
    }

    // printf("Terminal: %d rows, %d cols\r\n", row, col);
    if (system_config.terminal_ansi_rows != row || system_config.terminal_ansi_columns != col) {
        printf("Screen Resolution changed\r\n");
    }
    if (row == 0 || col == 0) {
        // non-detection fallback
        system_config.terminal_ansi_statusbar = 0;
        system_config.terminal_ansi_color = UI_TERM_NO_COLOR;
    } else {
        system_config.terminal_ansi_rows = row;
        system_config.terminal_ansi_columns = col;
    }
}

void ui_term_init(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {

        printf("\x1b[?3l"); // 80 columns
        printf("\x1b]0;%s\x1b\\", BP_HARDWARE_VERSION);
        // reset all styling
        printf("\e[0m");
        // set cursor type
        printf("\e[3 q");
        // clear screen
        printf("\e[2J");
        // set scroll region
        printf("\e[%d;%dr", 1, system_config.terminal_ansi_rows - 4);
    }
}

/*
\x1b  escape
[0m   reset/normal
[38;2;<r>;<g>;<b>m  set rgb text color
[48;2;<r>;<g>;<b>m  set rgb background color
[38;2;<r>;<g>;<b>;48;2;<r>;<g>;<b>m set text and background color
\x1bP$qm\x1b\\  query current settings, can be used to test true color support on SOME terminals... doesn't seem to be
widly used

*/

void ui_term_color_text(uint32_t rgb) {
    switch (system_config.terminal_ansi_color) {
        case UI_TERM_FULL_COLOR:
            printf("\x1b[38;2;%d;%d;%dm", (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)(rgb));
            break;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            printf("\x1b[38;5;%hhdm", ansi256_from_rgb(rgb));
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
            printf("\x1b[48;2;%d;%d;%dm", (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)(rgb));
            break;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            printf("\x1b[48;5;%hhdm", ansi256_from_rgb(rgb));
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
            count = printf("\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
                           (uint8_t)(rgb_text >> 16),
                           (uint8_t)(rgb_text >> 8),
                           (uint8_t)(rgb_text),
                           (uint8_t)(rgb_background >> 16),
                           (uint8_t)(rgb_background >> 8),
                           (uint8_t)(rgb_background));
            return count;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            count = printf("\x1b[38;5;%hhd;48;5;%hhdm", ansi256_from_rgb(rgb_text), ansi256_from_rgb(rgb_background));
            return count;
#endif
        case UI_TERM_NO_COLOR:
        default:
            break;
    }

    return 0;
}

uint32_t ui_term_color_text_background_buf(char* buf, uint32_t rgb_text, uint32_t rgb_background) {
    uint32_t count = 0;
    switch (system_config.terminal_ansi_color) {
        case UI_TERM_FULL_COLOR:
            count = sprintf(buf,
                            "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm",
                            (uint8_t)(rgb_text >> 16),
                            (uint8_t)(rgb_text >> 8),
                            (uint8_t)(rgb_text),
                            (uint8_t)(rgb_background >> 16),
                            (uint8_t)(rgb_background >> 8),
                            (uint8_t)(rgb_background));
            return count;
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
            count =
                sprintf(buf, "\x1b[38;5;%hhd;48;5;%hhdm", ansi256_from_rgb(rgb_text), ansi256_from_rgb(rgb_background));
            return count;
#endif
        case UI_TERM_NO_COLOR:
        default:
            break;
    }

    return 0;
}

#define UI_TERM_FULL_COLOR_CONCAT_TEXT(color) ("\x1b[38;2;" color "m")
#define UI_TERM_FULL_COLOR_CONCAT_BACKGROUND(color) ("\x1b[48;2;" color "m")
#define UI_TERM_256_COLOR_CONCAT_TEXT(color) ("\x1b[38;5;" color "m")
#define UI_TERM_256_COLOR_CONCAT_BACKGROUND(color) ("\x1b[48;5;" color "m")

char* ui_term_color_reset(void) {
    switch (system_config.terminal_ansi_color) {
#ifdef ANSI_COLOR_256
        case UI_TERM_256:
#endif
        case UI_TERM_FULL_COLOR:
            return "\x1b[0m";
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
    return system_config.terminal_ansi_color ? "\e[?25l" : "";
}
char* ui_term_cursor_show(void) {
    return !system_config.terminal_hide_cursor && system_config.terminal_ansi_color ? "\e[?25h" : "";
}

// handles the user input
uint32_t ui_term_get_user_input(void) {
    char c;

    if (!rx_fifo_try_get(&c)) {
        return 0;
    }

    switch (c) {
        case 0x09: // tab
            printf("\x07TAB\r\n");
            break;
        case 0x08: // backspace
            if (!ui_term_cmdln_char_backspace()) {
                printf("\x07");
            }
            break;
        case 0x7F: // delete
            if (!ui_term_cmdln_char_delete()) {
                printf("\x07");
            }
            break;
        case '\x1B': // escape commands
            rx_fifo_get_blocking(&c);
            switch (c) {
                case '[': // arrow keys
                    rx_fifo_get_blocking(&c);
                    ui_term_cmdln_arrow_keys(&c);
                    break;
                case 'O': // function keys (VT100 type)
                    rx_fifo_get_blocking(&c);
                    ui_term_cmdln_fkey(&c);
                    break;
                default:
                    break;
            }
            break;
        case '\r': // enter! go!
            if (cmdln_try_add(0x00)) {
                return 0xff;
            } else {
                printf("\x07");
            }
            break;
        default:
            if ((c < ' ') || (c > '~') ||
                !ui_term_cmdln_char_insert(&c)) // only accept printable characters if room available
            {
                printf("\x07");
            }
            break;
    }

    return 1;
}

bool ui_term_cmdln_char_insert(char* c) {
    if (cmdln.cursptr == cmdln.wptr) // at end of the command line, append new character
    {
        if (cmdln_pu(cmdln.wptr + 1) ==
            cmdln_pu(cmdln.rptr - 1)) // leave one extra space for final 0x00 command end indicator
        {
            return false;
        }

        tx_fifo_put(c);
        cmdln.buf[cmdln.wptr] = (*c);
        cmdln.wptr = cmdln_pu(cmdln.wptr + 1);
        cmdln.cursptr = cmdln.wptr;

    } else // middle of command line somewhere, insert new character
    {
        uint32_t temp = cmdln_pu(cmdln.wptr + 1);
        while (temp != cmdln.cursptr) // move each character ahead one position until we reach the cursor
        {
            cmdln.buf[temp] = cmdln.buf[cmdln_pu(temp - 1)];
            temp = cmdln_pu(temp - 1);
        }

        cmdln.buf[cmdln.cursptr] = (*c); // insert new character at the position

        temp = cmdln.cursptr; // write out all the characters to the user terminal after the cursor pointer to the write
                              // pointer
        while (temp != cmdln_pu(cmdln.wptr + 1)) {
            tx_fifo_put(&cmdln.buf[temp]);
            temp = cmdln_pu(temp + 1);
        }
        cmdln.cursptr = cmdln_pu(cmdln.cursptr + 1);
        cmdln.wptr = cmdln_pu(cmdln.wptr + 1);
        printf("\x1B[%dD", cmdln_pu(cmdln.wptr - cmdln.cursptr)); // return the cursor to the correct position
    }

    return true;
}

bool ui_term_cmdln_char_backspace(void) {
    if ((cmdln.wptr == cmdln.rptr) || (cmdln.cursptr == cmdln.rptr)) // not empty or at beginning?
    {
        return false;
    }

    if (cmdln.cursptr == cmdln.wptr) // at end?
    {
        cmdln.wptr = cmdln_pu(cmdln.wptr - 1); // write pointer back one space
        cmdln.cursptr = cmdln.wptr;            // cursor pointer also goes back one space
        printf("\x08 \x08");                   // back, space, back again
        // printf("\x1b[1X");
        cmdln.buf[cmdln.wptr] = 0x00; // is this really needed?
    } else {
        uint32_t temp = cmdln.cursptr;
        printf("\x1B[D"); // delete character on terminal
        while (temp != cmdln.wptr) {
            cmdln.buf[cmdln_pu(temp - 1)] =
                cmdln.buf[temp]; // write out the characters from cursor position to write pointer
            tx_fifo_put(&cmdln.buf[temp]);
            temp = cmdln_pu(temp + 1);
        }
        printf(" "); // get rid of trailing final character
        cmdln.buf[cmdln.wptr] = 0x00;
        cmdln.wptr = cmdln_pu(cmdln.wptr - 1); // move write and cursor positions back one space
        cmdln.cursptr = cmdln_pu(cmdln.cursptr - 1);
        printf("\x1B[%dD", cmdln_pu(cmdln.wptr - cmdln.cursptr + 1));
    }

    return true;
}

bool ui_term_cmdln_char_delete(void) {
    if ((cmdln.wptr == cmdln.rptr) || (cmdln.cursptr == cmdln.wptr)) // empty or at beginning?
    {
        return false;
    }

    uint32_t temp = cmdln.cursptr;
    while (temp != cmdln.wptr) // move each character ahead one position until we reach the cursor
    {
        cmdln.buf[temp] = cmdln.buf[cmdln_pu(temp + 1)];
        temp = cmdln_pu(temp + 1);
    }
    cmdln.buf[cmdln.wptr] = 0x00; // TODO: I dont think these are needed, it is done in the calling fucntion on <enter>
    cmdln.wptr = cmdln_pu(cmdln.wptr - 1);
    printf("\x1B[1P");

    return true;
}

void ui_term_cmdln_fkey(char* c) {
    switch ((*c)) {
        /*PF1 - Gold   ESC O P        ESC O P           F1
        PF2 - Help   ESC O Q        ESC O Q           F2
        PF3 - Next   ESC O R        ESC O R           F3
        PF4 - DelBrk ESC O S        ESC O S          F4*/
        case 'P':
            printf("F1");
            break;
        case 'Q':
            printf("F2");
            break;
        case 'R':
            printf("F3");
            break;
        case 'S':
            printf("F4");
            break;
    }
}

void ui_term_cmdln_arrow_keys(char* c) {
    char scrap;
    switch ((*c)) {
        case 'D':
            if (cmdln.cursptr != cmdln.rptr) // left
            {
                cmdln.cursptr = cmdln_pu(cmdln.cursptr - 1);
                printf("\x1B[D");
            } else {
                printf("\x07");
            }
            break;
        case 'C':
            if (cmdln.cursptr != cmdln.wptr) // right
            {
                cmdln.cursptr = cmdln_pu(cmdln.cursptr + 1);
                printf("\x1B[C");
            } else {
                printf("\x07");
            }
            break;
        case 'A': // up
            cmdln.histptr++;
            if (!ui_term_cmdln_history(cmdln.histptr)) // on error restore ptr and ring a bell
            {
                cmdln.histptr--;
                printf("\x07");
            }
            break;
        case 'B': // down
            cmdln.histptr--;
            if ((cmdln.histptr < 1) || (!ui_term_cmdln_history(cmdln.histptr))) {
                cmdln.histptr = 0;
                printf("\x07");
            }
            break;
        case '3': // 3~=delete
            rx_fifo_get_blocking(c);
            rx_fifo_get_blocking(&scrap);
            if (scrap != '~') {
                break;
            }
            if (!ui_term_cmdln_char_delete()) {
                printf("\x07");
            }
            break;
        case '4': // 4~=end
            rx_fifo_get_blocking(c);
            if (*c != '~') {
                break;
            }
            // end of cursor
            while (cmdln.cursptr != cmdln.wptr) {
                cmdln.cursptr = cmdln_pu(cmdln.cursptr + 1);
                printf("\x1B[C");
            }
            break;
            break;
        case '1': // teraterm style function key
            rx_fifo_get_blocking(c);
            if (*c == '~') {
                // home
                while (cmdln.cursptr != cmdln.rptr) {
                    cmdln.cursptr = cmdln_pu(cmdln.cursptr - 1);
                    printf("\x1B[D");
                }
                break;
            }
            rx_fifo_get_blocking(&scrap);
            if (scrap != '~') {
                break;
            }
            printf("TeraTerm F%c\r\n", *c);
            break;
        default:
            break;
    }
}

// copies a previous cmd to current position int ui_cmdbuff
int ui_term_cmdln_history(int ptr) {
    int i;
    uint32_t temp;

    i = 1;

    for (temp = cmdln_pu(cmdln.rptr - 2); temp != cmdln.wptr; temp = cmdln_pu(temp - 1)) {
        if (!cmdln.buf[temp]) {
            ptr--;
        }

        if ((ptr == 0) && (cmdln.buf[cmdln_pu(temp + 1)])) // do we want this one?
        {
            while (cmdln.cursptr != cmdln_pu(cmdln.wptr)) // clear line to end
            {
                printf(" ");
                cmdln.cursptr = cmdln_pu(cmdln.cursptr + 1);
            }
            while (cmdln.cursptr != cmdln.rptr) // TODO: verify		//move back to start;
            {
                printf("\x1B[D \x1B[D");
                cmdln.cursptr = cmdln_pu(cmdln.cursptr - 1);
            }

            while (cmdln.buf[cmdln_pu(temp + i)]) {
                cmdln.buf[cmdln_pu(cmdln.rptr + i - 1)] = cmdln.buf[cmdln_pu(temp + i)];
                tx_fifo_put(&cmdln.buf[cmdln_pu(temp + i)]);
                i++;
            }
            cmdln.wptr = cmdln_pu(cmdln.rptr + i - 1);
            cmdln.cursptr = cmdln.wptr;
            cmdln.buf[cmdln.wptr] = 0x00;
            break;
        }
    }

    return (!ptr);
}

void ui_term_progress_bar(uint32_t current, uint32_t total) {
    uint32_t pct = (current * 20) / (total);
    printf("\r%s[", ui_term_color_prompt());
    for (int8_t i = 0; i < 20; i++) {
        if (pct < i) {
            if (i % 2) {
                printf(" ");
            } else {
                printf("o");
            }
        } else if (pct == i) {
            printf("%sc", ui_term_color_notice());
        } else if (pct > i) {
            printf("-");
        }
    }
    printf("%s]\r\e[1C", ui_term_color_prompt());
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
    printf("%s]\r\e[1C", ui_term_color_prompt());
    pb->indicator_state = 1;
    pb->previous_pct = 0;
    pb->progress_cnt = 0;
}

void ui_term_progress_bar_update(uint32_t current, uint32_t total, ui_term_progress_bar_t* pb) {
    uint32_t pct = ((current) * 20) / (total);
    uint32_t previous_pct = pct - pb->previous_pct;

    system_config.terminal_ansi_statusbar_pause = true;
    if ((previous_pct) > 0) {
        for (uint8_t i = 0; i < (previous_pct); i++) // advance this many positions
        {
            printf("%s-", ui_term_color_prompt());
        }
    }

    if ((pb->progress_cnt > 600) || ((previous_pct) > 0)) // gone 5 loops without an advance
    {
        printf("%s%c\e[1D", ui_term_color_pacman(), (pb->indicator_state) ? 'C' : 'c'); // C and reset the cursor
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