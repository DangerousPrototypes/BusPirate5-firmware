#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "system_config.h"
#include "debug.h"
#include "usb_rx.h"

// clang-format off
struct _debug_uart debug_uart[] =
{
    { BP_DEBUG_UART_0, BP_DEBUG_UART_0_RX, BP_DEBUG_UART_0_TX, UART0_IRQ },
    { BP_DEBUG_UART_1, BP_DEBUG_UART_1_RX, BP_DEBUG_UART_1_TX, UART1_IRQ },
};
// clang-format off

static const char debug_pin_labels[][5] = {
    "DBTX",
    "DBRX",
    "DTTX",
    "DTRX",
};

// easy to remember debug function for developers...
void debug_tx(char c) {
    uart_putc(debug_uart[system_config.debug_uart_number].uart, c);
}
// TODO: currently blocking function, make nonblocking
bool debug_rx(char* c) {
    if (uart_is_readable(debug_uart[system_config.debug_uart_number].uart)) {
        *c = uart_getc(debug_uart[system_config.debug_uart_number].uart);
        return true;
    }
    return false;
}

void debug_uart_init(int uart_number, bool dbrx, bool dbtx, bool terminal_label) {
    // Initialise debug UART
    uart_init(debug_uart[uart_number].uart, 115200);
    uart_set_fifo_enabled(debug_uart[uart_number].uart, true); // might set to false

    if (dbtx) {
        // manually setup the buffer direction control pin to output
        // becuase debug pins are ignored in debug mode
        bio_buf_pin_init(debug_uart[uart_number].tx_pin);
        // set the buffer direction
        bio_buf_output(debug_uart[uart_number].tx_pin); // output TX
        // Set the GPIO pin mux to the UART
        bio_set_function(debug_uart[uart_number].tx_pin, GPIO_FUNC_UART);
        // claim and label pin
        system_bio_claim(
            true, debug_uart[uart_number].tx_pin, BP_PIN_DEBUG, debug_pin_labels[(terminal_label * 2) + 0]);
    }

    if (dbrx) {
        // manually setup the buffer direction control pin to output
        // becuase debug pins are ignored in debug mode
        bio_buf_pin_init(debug_uart[uart_number].rx_pin);
        // set the buffer direction
        bio_buf_input(debug_uart[uart_number].rx_pin); // input RX
        // Set the GPIO pin mux to the UART
        bio_set_function(debug_uart[uart_number].rx_pin, GPIO_FUNC_UART);
        // claim and label pin
        system_bio_claim(
            true, debug_uart[uart_number].rx_pin, BP_PIN_DEBUG, debug_pin_labels[(terminal_label * 2) + 1]);
    }
}

void rx_uart_disable(void) {
    // cleanup uart hardware and pin
}