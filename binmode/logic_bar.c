
void la_redraw(uint32_t start_pos, uint32_t la_samples) {

    if (start_pos + 76 > la_samples) // no more samples to show on screen
    {
        start_pos = la_samples - 76;
    }

    // find the start point
    uint32_t sample_ptr; // number of samples
    sample_ptr = (la_ptr - la_samples) & 0x1ffff;
    sample_ptr = (sample_ptr + start_pos) & 0x1ffff;
    system_config.terminal_ansi_statusbar_pause = true;
    busy_wait_ms(1);
    system_config.terminal_hide_cursor = true; // prevent the status bar from showing the cursor again
    printf("%s", ui_term_cursor_hide());
    // save cursor
    printf("\e7");
    // draw timing marks
    // printf("%s\e[3A\r\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d\t\t\e[8X%d", ui_term_color_reset(), start_pos+6,
    // start_pos+6+(16*1), start_pos+6+(16*2), start_pos+6+(16*3), start_pos+6+(16*4));

    printf("%s\e[%d;0H\e[K   \t%d\t\t%d\t\t%d\t\t%d\t\t%d",
           ui_term_color_reset(),
           system_config.terminal_ansi_rows - (12),
           start_pos + 6,
           start_pos + 6 + (16 * 1),
           start_pos + 6 + (16 * 2),
           start_pos + 6 + (16 * 3),
           start_pos + 6 + (16 * 4));

    for (int i = 0; i < 76; i++) {
        uint8_t sample, previous_sample;

        sample = la_buf[sample_ptr];

        sample_ptr++;
        sample_ptr &= 0x1ffff;

        printf("\e[%d;%dH", system_config.terminal_ansi_rows - (11), i + 3); // line graph top, current position

        for (int pins = 0; pins < 8; pins++) {
            if (sample & (0b1 << pins)) {
                // if(!(previous_sample & (0b1<<pins))) //rising edge
                //{
                // printf("%s\u250C", ui_term_color_prompt());
                //}
                // else
                //{
                // printf("%s\u2B1B", ui_term_color_prompt());
                ui_term_color_text_background(hw_pin_label_ordered_color[pins + 1][0],
                                              hw_pin_label_ordered_color[pins + 1][1]);
                printf(" %s", ui_term_color_reset());
                // ui_term_color_text(hw_pin_label_ordered_color[pins+1][1]);
                // printf("\u2500");
                //}
            } else {
                // if((previous_sample & (0b1<<pins))) //falling edge
                //{
                //     printf("%s\u2510", ui_term_color_error());
                // }
                // else
                //{

                printf("%s_", ui_term_color_error());
                //}
            }
            printf("\e[1B\e[1D"); // move one line down, one position left
        }
        previous_sample = sample;
    }
    printf("\e8"); // restore cursor
    system_config.terminal_ansi_statusbar_pause = false;
    system_config.terminal_hide_cursor = false;
    printf("%s", ui_term_cursor_show());
}

void la_draw_frame(void) {
    // printf("\r\nCommands: (r)un, (s)ave, e(x)it, arrow keys to navigate\r\n");
    // 80 characters wide box outline
    // box top and corners
    system_config.terminal_ansi_statusbar_pause = true;
    busy_wait_ms(1);
    system_config.terminal_hide_cursor = true; // prevent the status bar from showing the cursor again
    printf("%s", ui_term_cursor_hide());

    for (uint8_t i = 0; i < 10; i++) {
        printf("\r\n"); // make space!
    }

    // set scroll region, disable line wrap
    printf("\e[%d;%dr\e[7l", 1, system_config.terminal_ansi_rows - 14);

    // a little header thing?
    printf("\e[%d;0H\e[K\u253C", system_config.terminal_ansi_rows - (13)); // row 10 of LA
    for (int i = 0; i < 78; i++) {
        printf("\u2500");
    }
    printf("\u253c");

    // sample numbers, row 9 of LA
    printf("\e[%d;0H\e[K   \t0000\t\t1000\t\t2000\t\t4000\t\t5000", system_config.terminal_ansi_rows - (12));

    // box left and right
    // 8 bars start at monitor area (+3)
    // todo: lower if monitor bar disabled?
    for (int i = 0; i < 8; i++) // row 8 to 1 of LA
    {
        printf("\e[%d;0H\e[K", system_config.terminal_ansi_rows - (11 - i));
        ui_term_color_text_background(hw_pin_label_ordered_color[i + 1][0], hw_pin_label_ordered_color[i + 1][1]);
        printf(" %d%s\e[76C", i, ui_term_color_reset());
        ui_term_color_text_background(hw_pin_label_ordered_color[i + 1][0], hw_pin_label_ordered_color[i + 1][1]);
        printf("%d %s", i, ui_term_color_reset());
    }

    printf("\e[%d;0H\e[K", system_config.terminal_ansi_rows - (14)); // return to non-scroll area
    system_config.terminal_hide_cursor = false;
    printf("%s", ui_term_cursor_show());
}

uint32_t la_freq = 1000, la_samples = 1000;
uint32_t la_trigger_pin = 0, la_trigger_level = 0;

bool la_active = false;

void la_periodic(void) {
    if (la_active && la_status == LA_IDLE) {
        la_redraw(0, la_samples);
        logic_analyzer_arm((float)(la_freq * 1000), la_samples, la_trigger_pin, la_trigger_level, false);
    }
}

void la_test_args(struct command_result* res) {

    uint32_t sample_position = 0;

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
        la_draw_frame();
        la_active = true;
        amux_sweep();
        if (hw_adc_voltage[HW_ADC_MUX_VREG_OUT] < 100) {
            printf("%s", t[T_WARN_VOUT_VREF_LOW]);
        }
        return;
    }

    // goto la_sample;

    while (true) {
        char c;

        if (rx_fifo_try_get(&c)) {
            switch (c) {
                case 's': // TODO: need to handle wrap...
                    // storage_save_binary_blob_rollover();
                    storage_save_binary_blob_rollover(la_buf, (la_ptr - la_samples) & 0x1ffff, la_samples, 0x1ffff);
                    break;
                case 'r':
                la_sample:
                    logic_analyzer_arm((float)(la_freq * 1000), la_samples, la_trigger_pin, la_trigger_level, false);
                    sample_position = 0;
                    while (!logic_analyzer_is_done()) {
                        char c;
                        if (rx_fifo_try_get(&c)) {
                            if (c == 'x') {
                                printf("Canceled!\r\n");
                                goto la_x;
                            }
                        }
                    }
                    la_redraw(sample_position, la_samples);
                    logicanalyzer_reset_led();
                    break;
                case 'q':
                case 'x':
                la_x:
                    system_config.terminal_hide_cursor = false;
                    printf("\e[?25h\e[9B%s%s", ui_term_color_reset(), ui_term_cursor_show()); // back to bottom
                    logic_analyzer_cleanup();
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