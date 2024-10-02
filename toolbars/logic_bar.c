#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "opt_args.h"
#include "binmode/logicanalyzer.h"
#include "hardware/pio.h"
#include "logicanalyzer.pio.h"
#include "pirate/mem.h"
#include "hardware/structs/bus_ctrl.h"
#include "ui/ui_term.h"
#include "usb_rx.h"
#include "pirate/storage.h"
#include "pirate/rgb.h"
#include "pico/multicore.h"
#include "pirate/amux.h"
#include "ui/ui_cmdln.h"
#include "pirate/intercore_helpers.h"
#include "binmode/logicanalyzer.h"
#include "binmode/fala.h"

// 80 characters wide box outline
// box top and corners
#define LOGIC_BAR_WIDTH 80
#define LOGIC_BAR_HEIGHT 10
#define LOGIC_BAR_VERTICAL_LABELS 2 // width of each vertical label
#define LOGIC_BAR_GRAPH_WIDTH LOGIC_BAR_WIDTH - (LOGIC_BAR_VERTICAL_LABELS * 2)

uint32_t la_freq = 1000, la_samples = 1000;
uint32_t la_trigger_pin = 0, la_trigger_level = 0;
char logic_graph_low_character = '_';
char logic_graph_high_character = '#';

void logic_bar_config(char low, char high) {
    if(low!=0) logic_graph_low_character = low;
    if(high!=0) logic_graph_high_character = high;
}

// wrangle the terminal into a state where we can draw a nice box
void draw_prepare(void) {
    system_config.terminal_ansi_statusbar_pause = true;
    busy_wait_ms(1);
    system_config.terminal_hide_cursor = true; // prevent the status bar from showing the cursor again
    printf("%s", ui_term_cursor_hide());
}

void draw_release(void) {
    system_config.terminal_hide_cursor = false;
    printf("%s", ui_term_cursor_show());
    system_config.terminal_ansi_statusbar_pause = false;
}

// TODO: move to a central dispatch
uint16_t draw_get_position_index(uint16_t height) {
    // height of the logic bar, plus height of the status bar if active
    return system_config.terminal_ansi_rows - ((height) + (system_config.terminal_ansi_statusbar * 4));
}

void graph_timeline(uint16_t position, uint32_t start_pos) {
    // draw timing marks
    printf("%s\e[%d;0H\e[K   \t%d\t\t%d\t\t%d\t\t%d\t\t%d",
           ui_term_color_reset(),
           position,
           start_pos + 6,
           start_pos + 6 + (16 * 1),
           start_pos + 6 + (16 * 2),
           start_pos + 6 + (16 * 3),
           start_pos + 6 + (16 * 4));
}

// less memory access, but more terminal cursor movement
void graph_logic_lines_1(uint16_t position, uint32_t sample_ptr) {
    // draw the logic bars
    uint8_t sample;
    for (int i = 0; i < LOGIC_BAR_GRAPH_WIDTH; i++) {
        sample = logic_analyzer_read_ptr(sample_ptr);
        sample_ptr++;
        sample_ptr &= 0x1ffff;
        printf("\e[%d;%dH", position, i + 3); // line graph top, current position
        printf("%s", ui_term_color_error());
        for (int pins = 0; pins < 8; pins++) {
            if (sample & (0b1 << pins)) {
                printf("*");
            } else {
                printf("_");
            }
            printf("\e[1B\e[1D"); // move one line down, one position left
        }
    }
}

// more memory access, but less terminal cursor movement
void graph_logic_lines_2(uint16_t position, uint32_t sample_ptr) {

    printf("%s", ui_term_color_error());
    // draw the logic bars
    uint8_t sample;
    for (int pins = 0; pins < 8; pins++) {
        printf("\e[%d;%dH", position + pins, LOGIC_BAR_VERTICAL_LABELS + 1); // line graph top, current position
        uint32_t sample_ptr_temp = sample_ptr;
        for (int i = 0; i < LOGIC_BAR_GRAPH_WIDTH; i++) {
            sample = logic_analyzer_read_ptr(sample_ptr_temp);
            sample_ptr_temp++;
            sample_ptr_temp &= 0x1ffff;

            if (sample & (0b1 << pins)) {
                printf("%c", logic_graph_high_character);
            } else {
                printf("%c", logic_graph_low_character);
            }

            // printf("\e[1B\e[1D"); // move one line down, one position left
        }
    }
}

// TODO: either an exposed struct, or a function to access all the la variables
void logic_bar_redraw(uint32_t start_pos, uint32_t total_samples) {

    // find the start point
    uint32_t la_ptr =  logic_analyzer_get_end_ptr(); 

    if (total_samples < LOGIC_BAR_GRAPH_WIDTH) {
        // add zero padding...
    } else if (start_pos + (LOGIC_BAR_GRAPH_WIDTH) >
               total_samples) { // recenter if we;re off to the right side of the data
        start_pos = total_samples - LOGIC_BAR_GRAPH_WIDTH;
    }

    uint32_t sample_ptr = logic_analyzer_get_start_ptr(total_samples);
    sample_ptr = (sample_ptr + start_pos) & 0x1ffff;
    // printf("la_prt: %d, sample_ptr: %d\r\n", la_ptr, sample_ptr);
    //  freeze terminal updates
    draw_prepare();

    // save cursor
    printf("\e7");

    // draw timing marks
    uint16_t position = draw_get_position_index(LOGIC_BAR_HEIGHT);
    graph_timeline(position + 2, start_pos);

    // draw the logic bars
    // graph_logic_lines_1(position+3, sample_ptr);
    graph_logic_lines_2(position + 3, sample_ptr);

    // restore cursor
    printf("\e8");

    draw_release();
}

// add blank space
void frame_blank(uint16_t height) {
    // add space to draw the box
    for (uint8_t i = 0; i < height; i++) {
        printf("\r\n"); // make space!
    }
}

// a little header thing
// +------------------+
void frame_top(uint16_t position, uint16_t width) {
    printf("\e[%d;0H\e[K\u253C", position); // row 10 of LA
    for (int i = 0; i < width - 2; i++) {
        printf("\u2500");
    }
    printf("\u253c");
}

// todo: pass actual sample numbers
void frame_sample_numbers(uint16_t position) {
    printf("\e[%d;0H\e[K   \t0000\t\t1000\t\t2000\t\t4000\t\t5000", position);
}

void frame_vertical_labels(uint16_t position) {
    for (int i = 0; i < 8; i++) { // row 8 to 1 of LA
        printf("\e[%d;0H\e[K", position + i);
        ui_term_color_text_background(hw_pin_label_ordered_color[i + 1][0], hw_pin_label_ordered_color[i + 1][1]);
        printf(" %d%s\e[76C", i, ui_term_color_reset());
        ui_term_color_text_background(hw_pin_label_ordered_color[i + 1][0], hw_pin_label_ordered_color[i + 1][1]);
        printf("%d %s", i, ui_term_color_reset());
    }
}

void logic_bar_draw_frame(void) {
    // height of the logic bar, plus height of the status bar if active
    // todo: allocate position index from central toolbar logic
    uint16_t toolbar_position_index = draw_get_position_index(LOGIC_BAR_HEIGHT);

    // freeze terminal updates
    draw_prepare();

    // blank space
    frame_blank(LOGIC_BAR_HEIGHT);

    // set scroll region, disable line wrap
    printf("\e[%d;%dr\e[7l", 1, toolbar_position_index);

    // draw box top
    frame_top(toolbar_position_index + 1, LOGIC_BAR_WIDTH);

    // sample numbers, row 9 of LA
    frame_sample_numbers(toolbar_position_index + 2);

    // box left and right
    // 8 bars start at monitor area (+3)
    frame_vertical_labels(toolbar_position_index + 3);

    // return to non-scroll area
    printf("\e[%d;0H\e[K", toolbar_position_index); // return to non-scroll area
    draw_release();
}

//detach/release/stop/end the logic bar frame
void logic_bar_detach(void) {
    //  freeze terminal updates
    draw_prepare();

    // save cursor
    //printf("\e7");

    uint16_t position = draw_get_position_index(LOGIC_BAR_HEIGHT);
    
    // set scroll region, disable line wrap
    printf("\e[%d;%dr\e[7l\r\n\r\n", 1, position+LOGIC_BAR_HEIGHT);
    

    frame_blank(LOGIC_BAR_HEIGHT);

    // restore cursor
    //printf("\e8");

    draw_release();

    printf("\e[?25h\e[9B%s%s", ui_term_color_reset(), ui_term_cursor_show()); // back to bottom
}

bool logic_bar_visible = false;

void logic_bar_update(void) {
    if(!logic_bar_visible) return;
    uint32_t total_samples = logic_analyzer_get_end_ptr(); // TODO: REMOVE HACK
    logic_bar_redraw(0, total_samples);
}

bool logic_bar_start(void) {
    //this should setup and activate hooks for fala if not already...
    if(!fala_notify_register(&logic_bar_update)) return false;
    logic_bar_draw_frame();
    logic_bar_visible = true;
    return true;
}

void logic_bar_stop(void) {
    //this should stop and cleanup fala if not already...
    fala_notify_unregister(&logic_bar_update);
    logic_bar_detach();
    logic_bar_visible = false;

}

void logic_bar_hide(void) {
    logic_bar_detach();
    logic_bar_visible = false;
 }

void logic_bar_show(void) {
    logic_bar_draw_frame();
    logic_bar_update();
    logic_bar_visible = true;
}

uint32_t sample_position = 0;
void logic_bar_navigate(void) {
    printf("\r\n%sCommands: <- and -> to scroll, x or q to exit%s\r\n", ui_term_color_info(), ui_term_color_reset()); //(r)un, (s)ave, 
    // find the start point
    //for follow along logic analyzer: the end pointer = number of samples
    //TODO: switch is using normal analyzer mode with fixed samples and triggers
    uint32_t total_samples = logic_analyzer_get_end_ptr();

    if(!logic_bar_visible){
        logic_bar_draw_frame();
        logic_bar_redraw(0, total_samples);
    }

    while (true) {
        char c;

        if (!rx_fifo_try_get(&c)) {
            continue;
        }

        switch (c) {
            case 's': // TODO: need to handle wrap...
                // storage_save_binary_blob_rollover();
                // storage_save_binary_blob_rollover(la_buf, (la_ptr - la_samples) & 0x1ffff, la_samples, 0x1ffff);
                break;
            case 'r':
            la_sample:
                // logic_analyzer_arm((float)(la_freq * 1000), la_samples, la_trigger_pin, la_trigger_level, false);
                //sample_position = 0;
                /*while (!logic_analyzer_is_done()) {
                    char c;
                    if (rx_fifo_try_get(&c)) {
                        if (c == 'x') {
                            printf("Canceled!\r\n");
                            goto la_x;
                        }
                    }
                }*/
                //logic_bar_redraw(sample_position, total_samples);
                // logicanalyzer_reset_led();
                break;
            case 'q':
            case 'x':
            la_x:
                //system_config.terminal_hide_cursor = false;
                printf("\e[?25h\e[9B%s%s", ui_term_color_reset(), ui_term_cursor_show()); // back to bottom
                return;
                break;
            case '\x1B': // escape commands
                rx_fifo_get_blocking(&c);
                switch (c) {
                    case '[': // arrow keys
                        rx_fifo_get_blocking(&c);
                        switch (c) {
                            case 'D': // left
                                if (sample_position < 64) {
                                    sample_position = 0;
                                } else {
                                    sample_position -= 64;
                                }
                                logic_bar_redraw(sample_position, total_samples);
                                break;
                            case 'C':                                  // right
                                if(total_samples < 76) { //not enough samples to scroll
                                    sample_position = 0;
                                } else if (sample_position > (total_samples - 63)) {// samples - columns
                                    sample_position = total_samples - 63;
                                } else {
                                    sample_position += 64;
                                }
                                logic_bar_redraw(sample_position, total_samples);
                                break;
                        }
                        break;
                }
                break;
        }
    }
}

#if 0
/*void la_periodic(void) {
    if (la_active && la_status == LA_IDLE) {
        la_redraw(0, la_samples);
        logic_analyzer_arm((float)(la_freq * 1000), la_samples, la_trigger_pin, la_trigger_level, false);
    }
}



void logic_bar(void) {

    uint32_t sample_position = 0;
    logic_bar_draw_frame();
    //la_redraw(sample_position, la_samples);



    if (!la_active) {
        if (!logicanalyzer_setup()) {
            printf("Failed to allocate buffer. Is the scope running?\r\n");
        }
    }

    command_var_t arg;
    uint32_t temp;
    cmdln_args_find_flag_uint32('f', &arg, &temp);
    if (arg.has_value) // freq in khz
    {
        la_freq = temp;
    }
    printf("Freq: %dkHz ", la_freq);
    cmdln_args_find_flag_uint32('s', &arg, &temp);
    if (arg.has_value) // samples
    {
        la_samples = temp;
    }
    printf("Samples: %d ", la_samples);

    cmdln_args_find_flag_uint32('t', &arg, &temp);
    if (arg.has_value) // trigger pin (or none)
    {
        if (temp >= 0 && temp <= 7) {
            la_trigger_pin = 1u << temp;
            printf("Trigger pin: IO%d ", temp);
            cmdln_args_find_flag_uint32('l', &arg, &temp);
            if (arg.has_value) // trigger level
            {
                la_trigger_level = temp ? 1u << temp : 0;
                printf("Trigger level: %d \r\n", temp);
            }
        } else {
            printf("Trigger pin: range error! Using none.");
        }
    }


    if (!la_active) {
        //la_draw_frame();
        la_active = true;
        /*amux_sweep();
        if (hw_adc_voltage[HW_ADC_MUX_VREG_OUT] < 100) {
            printf("%s", GET_T(T_WARN_VOUT_VREF_LOW));
        }*/
        return;
    }

    // goto la_sample;

    while (true) {
        char c;

        if (rx_fifo_try_get(&c)) {
            switch (c) {
                case 's': // TODO: need to handle wrap...
                    // storage_save_binary_blob_rollover();
                    //storage_save_binary_blob_rollover(la_buf, (la_ptr - la_samples) & 0x1ffff, la_samples, 0x1ffff);
                    break;
                case 'r':
                la_sample:
                    //logic_analyzer_arm((float)(la_freq * 1000), la_samples, la_trigger_pin, la_trigger_level, false);
                    sample_position = 0;
                    /*while (!logic_analyzer_is_done()) {
                        char c;
                        if (rx_fifo_try_get(&c)) {
                            if (c == 'x') {
                                printf("Canceled!\r\n");
                                goto la_x;
                            }
                        }
                    }*/
                    la_redraw(sample_position, la_samples);
                    //logicanalyzer_reset_led();
                    break;
                case 'q':
                case 'x':
                la_x:
                    system_config.terminal_hide_cursor = false;
                    printf("\e[?25h\e[9B%s%s", ui_term_color_reset(), ui_term_cursor_show()); // back to bottom
                    //logic_analyzer_cleanup();
                    return;
                    break;
                case '\x1B': // escape commands
                    rx_fifo_get_blocking(&c);
                    switch (c) {
                        case '[': // arrow keys
                            rx_fifo_get_blocking(&c);
                            switch (c) {
                                case 'D': // left
                                    if (sample_position < 64) {
                                        sample_position = 0;
                                    } else {
                                        sample_position -= 64;
                                    }
                                    la_redraw(sample_position, la_samples);
                                    break;
                                case 'C':                                  // right
                                    if (sample_position > la_samples - 76) // samples - columns
                                    {
                                        sample_position = la_samples - 76;
                                    } else {
                                        sample_position += 64;
                                    }
                                    la_redraw(sample_position, la_samples);
                                    break;
                            }
                            break;
                    }
                    break;
            }
        }
    }
}

#endif
