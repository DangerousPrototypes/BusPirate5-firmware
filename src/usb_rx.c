#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "hardware/dma.h"
// #include "buf.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "tusb.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "debug_uart.h"

// USB RX:
// This file contains the code for the user interface ring buffer
// and IO method. USB is the normal IO, but running a chip under debug
// on a USB attached device is a nightmare. BP_DEBUG_ENABLED In the /platform/ folder
// configuration file enables a UART debugging option. All user interface IO
// will be routed to one of the two UARTs (selectable) on the Bus Pirate buffered IO pins
// UART debug mode is way over engineered using DMA et al, and has some predelection for bugs
// in status bar updates

void rx_uart_irq_handler(void);

queue_t rx_fifo;
queue_t bin_rx_fifo;
#define RX_FIFO_LENGTH_IN_BITS 7 // 2^n buffer size. 2^3=8, 2^9=512
#define RX_FIFO_LENGTH_IN_BYTES (0x0001 << RX_FIFO_LENGTH_IN_BITS)
char rx_buf[RX_FIFO_LENGTH_IN_BYTES];
char bin_rx_buf[RX_FIFO_LENGTH_IN_BYTES];

// init buffer (and IRQ for UART debug mode)
void rx_fifo_init(void) {
    queue2_init(&rx_fifo, rx_buf, RX_FIFO_LENGTH_IN_BYTES); // buffer size must be 2^n for queue AND DMA rollover
    queue2_init(&bin_rx_fifo, bin_rx_buf, RX_FIFO_LENGTH_IN_BYTES);
}

// enables receive interrupt for ALREADY configured debug uarts (see init in debug.c)
void rx_uart_init_irq(void) {
    // rx interrupt
    irq_set_exclusive_handler(debug_uart[system_config.terminal_uart_number].irq, rx_uart_irq_handler);
    // irq_set_priority(debug_uart[system_config.terminal_uart_number].irq, 0xFF);
    irq_set_enabled(debug_uart[system_config.terminal_uart_number].irq, true);
    uart_set_irq_enables(debug_uart[system_config.terminal_uart_number].uart, true, false);
}

// UART interrupt handler
void rx_uart_irq_handler(void) {
    // while bytes available shove them in the buffer
    while (uart_is_readable(debug_uart[system_config.terminal_uart_number].uart)) {
        uint8_t c = uart_getc(debug_uart[system_config.terminal_uart_number].uart);
        queue2_add_blocking(&rx_fifo, &c);
    }
}

void rx_usb_init(void) {
    tusb_init();
}

// USB (tinyUSB) interrupt handler
// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
    char buf[64];

    if (itf == 0 && tud_cdc_n_available(0)) {
        uint32_t count = tud_cdc_n_read(0, buf, 64);

        // while bytes available shove them in the buffer
        for (uint8_t i = 0; i < count; i++) {
            queue2_add_blocking(&rx_fifo, &buf[i]);
        }
    }

    if (system_config.binmode_usb_rx_queue_enable && itf == 1 && tud_cdc_n_available(1)) {
        uint32_t count = tud_cdc_n_read(1, buf, 64);

        // while bytes available shove them in the buffer
        for (uint8_t i = 0; i < count; i++) {
            queue2_add_blocking(&bin_rx_fifo, &buf[i]);
        }
    }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    system_config.rts = rts;
}

// insert a byte into the queue
void rx_fifo_add(char* c) {
    queue2_add_blocking(&rx_fifo, c);
}

// functions to access the ring buffer from other code
// block until a byte is available, remove from buffer
void rx_fifo_get_blocking(char* c) {
    queue2_remove_blocking(&rx_fifo, c);
}
// try to get a byte, remove from buffer if available, return false if no byte
bool rx_fifo_try_get(char* c) {
    return queue2_try_remove(&rx_fifo, c);
}
// block until a byte is available, return byte but leave in buffer
void rx_fifo_peek_blocking(char* c) {
    queue2_peek_blocking(&rx_fifo, c);
}
// try to peek at next byte, return byte but leave in buffer, return false if no byte
bool rx_fifo_try_peek(char* c) {
    return queue2_try_peek(&rx_fifo, c);
}

// BINMODE queue
void bin_rx_fifo_add(char* c) {
    queue2_add_blocking(&bin_rx_fifo, c);
}

void bin_rx_fifo_get_blocking(char* c) {
    queue2_remove_blocking(&bin_rx_fifo, c);
}

void bin_rx_fifo_available_bytes(uint16_t* cnt) {
    queue_available_bytes(&bin_rx_fifo, cnt);
}

bool bin_rx_fifo_try_get(char* c) {
    bool result = queue2_try_remove(&bin_rx_fifo, c);
    // if(result) printf("%.2x ", (*c));
    return result;
}