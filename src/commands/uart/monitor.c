#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
// #include "usb_rx.h"
// #include "usb_tx.h"
#include "pirate/bio.h"
#include "lib/bp_args/bp_cmd.h"

static const char pin_labels[][5] = { "TX->", "RX<-", "CTS", "RTS"

};

static const char* const usage[] = { "test\t[-h(elp)] [-t(oolbar)]",
                                     "Test Dual RS232 plank:%s test"};

static const bp_command_opt_t monitor_opts[] = {
    { "toolbar", 't', BP_ARG_NONE, NULL, 0, NULL },
    { 0 }
};

const bp_command_def_t uart_monitor_def = {
    .name = "monitor",
    .description = T_UART_CMD_TEST,
    .opts = monitor_opts,
    .usage = usage,
    .usage_count = count_of(usage),
};

bool uart_timer_callback(struct repeating_timer* t) {
    static int cnt = 0;
    if (cnt % 2) {
        uart_putc_raw(uart0, 'A' + cnt);
    } else {
        uart_putc_raw(uart1, 'A' + cnt);
    }
    cnt++;
    if (cnt > 25) {
        cnt = 0;
    }
    return true;
}
void monitor_plank_test(void);
void uart_monitor_handler(struct command_result* res) {
    if (bp_cmd_help_check(&uart_monitor_def, res->help_flag)) {
        return;
    }
    if (!ui_help_check_vout_vref()) {
        return;
    }

    bool toolbar_state = system_config.terminal_ansi_statusbar_pause;
    bool pause_toolbar = bp_cmd_find_flag(&uart_monitor_def, 't');
    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = true;
    }
    uint speed = 115200;
    uint data_bits = 8;
    uint stop_bits = 1;
    uint parity = UART_PARITY_NONE;
    // setup peripheral
    uart_init(uart1, speed);
    printf("\r\n%s%s: %u %s%s",
           ui_term_color_notice(),
           GET_T(T_UART_ACTUAL_SPEED_BAUD),
           speed,
           GET_T(T_UART_BAUD),
           ui_term_color_reset());
    uart_set_format(uart1, data_bits, stop_bits, parity);
    // set buffers to correct position
    bio_buf_output(BIO0); // tx
    bio_buf_input(BIO1);  // rx
    // assign peripheral to io pins
    bio_set_function(BIO0, GPIO_FUNC_UART); // tx
    bio_set_function(BIO1, GPIO_FUNC_UART); // rx
    system_bio_update_purpose_and_label(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_MODE, pin_labels[1]);

    // set buffers to correct position
    bio_buf_input(BIO2);  // cts uart1 input
    bio_buf_output(BIO3); // rts uart1 output
    bio_buf_input(BIO6);  // cts uart0
    bio_buf_output(BIO7); // rts uart0
    // assign peripheral to io pins
    bio_set_function(BIO2, GPIO_FUNC_UART);
    bio_set_function(BIO3, GPIO_FUNC_UART);
    bio_set_function(BIO6, GPIO_FUNC_UART);
    bio_set_function(BIO7, GPIO_FUNC_UART);
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_MODE, pin_labels[3]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[3]);

    printf("%s%s%s\r\n", ui_term_color_notice(), GET_T(T_HELP_UART_BRIDGE_EXIT), ui_term_color_reset());

    // add_repeating_timer_ms(-5000, uart_timer_callback, NULL, &uart_timer);
    busy_wait_ms(10);
    if (uart_is_readable(uart0)) {
        (void)uart_getc(uart0);
    }
    if (uart_is_readable(uart1)) {
        (void)uart_getc(uart1);
    }

    monitor_plank_test();
    return;

    while (true) {
        // exit when button pressed.
        if (button_get(0)) {
            break;
        }
    }

    // cancel_repeating_timer(&uart_timer);

    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = toolbar_state;
    }
}

// test dual rs232 plank (or raw pin connections)
// TODO: dual baord test with hgsemi
#define UART_DTE uart1
#define UART_DCE uart0

void monitor_fill(uart_inst_t* uart) {
    for (uint8_t i = 0; i < 8; i++) {
        uart_putc_raw(uart, 'A' + i);
        printf(" %c", 'A' + i);
    }
}

void monitor_verify(uart_inst_t* uart) {
    for (uint8_t i = 0; i < 8; i++) {
        if (uart_is_readable_within_us(uart, 1000)) {
            char c = uart_getc(uart);
            if (c == 'A' + i) {
                printf(" %c", c);
            } else {
                printf("\r\nError: expected %c, got %c\r\n", 'A' + i, c);
                return;
            }
        }
    }
}

void monitor_plank_test(void) {
    uart_set_fifo_enabled(UART_DTE, true);
    uart_set_fifo_enabled(UART_DCE, true);

    // flush fifos
    while (uart_is_readable(UART_DTE)) {
        uart_getc(UART_DTE);
    }
    while (uart_is_readable(UART_DCE)) {
        uart_getc(UART_DCE);
    }
    // DTE to DCE
    printf("DTE TX:");
    monitor_fill(UART_DTE);
    printf("\r\n");

    busy_wait_ms(100);
    printf("DCE RX:");
    monitor_verify(UART_DCE);
    printf("\r\n");

    // DCE to DTE
    printf("DCE TX:");
    monitor_fill(UART_DCE);
    printf("\r\n");

    busy_wait_ms(100);
    printf("DTE RX:");
    monitor_verify(UART_DTE);
    printf("\r\n");

    // enable flow control and CTS RTS
    uart_set_fifo_enabled(UART_DTE, false);
    uart_set_fifo_enabled(UART_DCE, false);
    uart_set_hw_flow(UART_DTE, true, true);
    uart_set_hw_flow(UART_DCE, true, true);
    // flush fifos
    while (uart_is_readable(UART_DTE)) {
        uart_getc(UART_DTE);
    }
    while (uart_is_readable(UART_DCE)) {
        uart_getc(UART_DCE);
    }

    // fill DCE buffer, check flow control
    uint8_t i = 0;
    printf("Flow control test\r\n");
    for (i = 0; i < 8; i++) {
        if (uart_is_writable(UART_DTE)) {
            uart_putc_raw(UART_DTE, 'A' + i);
            if (i == 7) {
                printf("Error: DTE flow control not active after %d bytes\r\n", i + 1);
                break;
            }
        } else {
            printf("DTE flow control active after %d bytes\r\n", i + 1);
            break;
        }
        busy_wait_ms(10);
    }
    // flush fifos
    while (uart_is_readable(UART_DTE)) {
        uart_getc(UART_DTE);
    }
    while (uart_is_readable(UART_DCE)) {
        uart_getc(UART_DCE);
    }

    for (i = 0; i < 8; i++) {
        if (uart_is_writable(UART_DCE)) {
            uart_putc_raw(UART_DCE, 'A' + i);
            if (i == 7) {
                printf("Error: DCE flow control not active after %d bytes\r\n", i + 1);
                break;
            }
        } else {
            printf("DCE flow control active after %d bytes\r\n", i + 1);
            break;
        }
        busy_wait_ms(10);
    }
    // flush fifos
    while (uart_is_readable(UART_DTE)) {
        uart_getc(UART_DTE);
    }
    while (uart_is_readable(UART_DCE)) {
        uart_getc(UART_DCE);
    }

    uart_set_hw_flow(UART_DTE, false, false);
    uart_set_hw_flow(UART_DCE, false, false);
}