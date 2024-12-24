#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "hardware/dma.h"
// #include "buf.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "tusb.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "debug_uart.h"

// USB TX:
// This file contains the code for the user interface ring buffer
// and IO method. USB is the normal IO, but running a chip under debug
// on a USB attached device is a nightmare. BP_DEBUG_ENABLED In the /platform/ folder
// configuration file enables a UART debugging option. All user interface IO
// will be routed to one of the two UARTs (selectable) on the Bus Pirate buffered IO pins
// UART debug mode is way over engineered using DMA et al, and has some predelection for bugs
// in status bar updates

// TODO: rework all the TX stuff into a nice struct with clearer naming
queue_t tx_fifo;
queue_t bin_tx_fifo;
#define TX_FIFO_LENGTH_IN_BITS 10 // 2^n buffer size. 2^3=8, 2^9=512
#define TX_FIFO_LENGTH_IN_BYTES (0x0001 << TX_FIFO_LENGTH_IN_BITS)
char tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(2048)));
char bin_tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(2048)));

#define MAXIMUM_STATUS_BAR_BUFFER_BYTES 1024
char tx_sb_buf[MAXIMUM_STATUS_BAR_BUFFER_BYTES];
uint16_t tx_sb_buf_cnt = 0;
uint16_t tx_sb_buf_index = 0;
bool tx_sb_buf_ready = false;

void tx_fifo_init(void) {
    // OK to call from either core
    queue2_init(&tx_fifo, tx_buf, TX_FIFO_LENGTH_IN_BYTES); // buffer size must be 2^n for queue AND DMA rollover
    queue2_init(&bin_tx_fifo, bin_tx_buf, TX_FIFO_LENGTH_IN_BYTES); // buffer size must be 2^n for queue AND DMA
                                                                    // rollover
}

void tx_sb_start(uint32_t valid_characters_in_status_bar) {

    BP_ASSERT_CORE1();
    BP_ASSERT(valid_characters_in_status_bar <= MAXIMUM_STATUS_BAR_BUFFER_BYTES);
    tx_sb_buf_cnt = valid_characters_in_status_bar;
    tx_sb_buf_ready = true;
}

void tx_fifo_service(void) {
    BP_ASSERT_CORE1(); // tx fifo is drained from core1 only

// state machine:
#define IDLE 0
#define STATUSBAR_DELAY 1
#define STATUSBAR_TX 2
    static uint8_t tx_state = IDLE;

    uint16_t bytes_available;
    char data[64];
    uint8_t i = 0;

    if (system_config.terminal_usb_enable) { // is tinyUSB CDC ready?
        if (tud_cdc_n_write_available(0) < 64) {
            return;
        }
    }

    switch (tx_state) {
        case IDLE:
            queue_available_bytes(&tx_fifo, &bytes_available);
            if (bytes_available) {
                i = 0;
                while (queue2_try_remove(&tx_fifo, &data[i])) {
                    i++;
                    if (i >= 64) {
                        break;
                    }
                }
                break; // break out of switch and continue below
            }

            // status bar update is ready
            if (tx_sb_buf_ready) {
                tx_state = STATUSBAR_DELAY;
                tx_sb_buf_index = 0;
                return; // return for next state
            }

            return; // nothing, just return

            break;
        case STATUSBAR_DELAY:
            // test: check that no bytes in tx_fifo minimum 2 cycles in a row
            // prevent the status bar from being wiped out by the VT100 setup commands
            // that might be pending in the TX FIFO
            queue_available_bytes(&tx_fifo, &bytes_available);
            tx_state = (bytes_available ? IDLE : STATUSBAR_TX);
            return; // return for next cycle

            break;
        case STATUSBAR_TX:
            // read out 64 bytes into data at a time until complete
            // TODO: pass a pointer to the array cause this is inefficient
            i = 0;
            while (tx_sb_buf_index < tx_sb_buf_cnt) {
                data[i] = tx_sb_buf[tx_sb_buf_index];
                tx_sb_buf_index++;
                i++;
                if (tx_sb_buf_index >= tx_sb_buf_cnt) {
                    tx_sb_buf_ready = false;
                    tx_state = IDLE; // done, next cycle go to idle
                    system_config.terminal_ansi_statusbar_update =
                        true; // after first draw of status bar, then allow updates by core1 service loop
                    break;
                }
                if (i >= 64) {
                    break;
                }
            }
            break;
        default:
            tx_state = IDLE;
            break;
    }

    // if(i==0) return; //safety check

    // write to terminal usb
    if (system_config.terminal_usb_enable) {
        tud_cdc_n_write(0, &data, i);
        tud_cdc_n_write_flush(0);
        if (system_config.terminal_uart_enable) {
            tud_task(); // makes it nicer if we service when the UART is enabled
        }
    }

    // write to terminal debug uart
    if (system_config.terminal_uart_enable) {
        for (uint8_t j = 0; j < i; j++) {
            uart_putc(debug_uart[system_config.terminal_uart_number].uart, data[j]);
        }
    }

    return;
}

void tx_fifo_put(char* c) {
    BP_ASSERT_CORE0(); // tx fifo shoudl only be added to from core 0 (deadlock risk)
    queue2_add_blocking(&tx_fifo, c);
}

void bin_tx_fifo_put(const char c) {
    BP_ASSERT_CORE0(); // tx fifo shoudl only be added to from core 0 (deadlock risk)
    queue2_add_blocking(&bin_tx_fifo, &c);
}

bool bin_tx_fifo_try_get(char* c) {
    BP_ASSERT_CORE1(); // tx fifo is drained from core1 only
    return queue2_try_remove(&bin_tx_fifo, c);
}

void bin_tx_fifo_service(void) {
    BP_ASSERT_CORE1(); // tx fifo is drained from core1 only

    uint16_t bytes_available;
    char data[64];
    uint8_t i = 0;

    // is tinyUSB CDC ready?
    if (tud_cdc_n_write_available(1) < 64) {
        return;
    }

    queue_available_bytes(&bin_tx_fifo, &bytes_available);
    if (bytes_available) {
        i = 0;
        while (queue2_try_remove(&bin_tx_fifo, &data[i])) {
            i++;
            if (i >= 64) {
                break;
            }
        }
    }

    tud_cdc_n_write(1, &data, i);
    tud_cdc_n_write_flush(1);
}

bool bin_tx_not_empty(void) {
    // OK to check empty from either core
    uint16_t cnt;
    // gets the number of additional bytes that can be added to the queue
    queue_available_bytes(&bin_tx_fifo, &cnt);
    // If that differs from the total size of the queue, then it's not empty
    return cnt != TX_FIFO_LENGTH_IN_BYTES;
}
