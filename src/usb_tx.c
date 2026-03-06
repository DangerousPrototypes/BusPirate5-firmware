/**
 * @file usb_tx.c
 * @brief USB and UART transmit buffer management
 * 
 * This file implements the transmit side of the user interface buffering system.
 * It handles both USB CDC and UART debug output, with support for VT100 toolbar
 * updates and DMA transfers.
 * 
 * The TX system uses lock-free SPSC ring buffers for regular output and toolbar output,
 * with a state machine to manage toolbar refresh without corrupting output.
 * 
 * Thread safety: Uses lock-free SPSC queues
 * - Producer: Core0 (printf output, command output)
 * - Consumer: Core1 (USB/UART transmission)
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "spsc_queue.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "tusb.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "debug_uart.h"

/** @defgroup tx_buffers TX Buffer Definitions
 * @brief Transmit ring buffers and status bar management
 * @{
 */

spsc_queue_t tx_fifo;              /**< Main transmit FIFO for regular output */
spsc_queue_t bin_tx_fifo;          /**< Binary mode transmit FIFO */

/** TX FIFO size in bits (2^10 = 1024 bytes) */
#define TX_FIFO_LENGTH_IN_BITS 10
/** TX FIFO size in bytes */
#define TX_FIFO_LENGTH_IN_BYTES (0x0001 << TX_FIFO_LENGTH_IN_BITS)

uint8_t tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(4)));     /**< TX buffer storage */
uint8_t bin_tx_buf[TX_FIFO_LENGTH_IN_BYTES] __attribute__((aligned(4))); /**< Binary TX buffer storage */

/** Maximum size of toolbar render buffer */
#define MAXIMUM_TOOLBAR_BUFFER_BYTES 1024
char tx_tb_buf[MAXIMUM_TOOLBAR_BUFFER_BYTES]; /**< Toolbar render buffer */
uint16_t tx_tb_buf_cnt = 0;                      /**< Number of valid characters in toolbar buffer */
uint16_t tx_tb_buf_index = 0;                    /**< Current position in toolbar buffer */
bool tx_tb_buf_ready = false;                    /**< Toolbar buffer ready to transmit */

/** @} */ // end of tx_buffers

/**
 * @brief Initialize transmit FIFOs
 * 
 * Sets up the lock-free SPSC ring buffers for regular and binary mode transmission.
 * Buffer sizes must be powers of 2 for efficient modulo operation.
 * 
 * @note Can be called from either core
 */
void tx_fifo_init(void) {
    // OK to call from either core
    spsc_queue_init(&tx_fifo, tx_buf, TX_FIFO_LENGTH_IN_BYTES);
    spsc_queue_init(&bin_tx_fifo, bin_tx_buf, TX_FIFO_LENGTH_IN_BYTES);
}

/**
 * @brief Mark status bar buffer as ready for transmission
 * 
 * @param len Number of valid characters in toolbar buffer
 * 
 * @pre Must be called from Core1
 * @pre len <= MAXIMUM_TOOLBAR_BUFFER_BYTES
 * 
 * @warning This function asserts if not called from Core1
 */
void tx_tb_start(uint32_t len) {

    BP_ASSERT_CORE1();
    BP_ASSERT(len <= MAXIMUM_TOOLBAR_BUFFER_BYTES);
    tx_tb_buf_cnt = len;
    tx_tb_buf_ready = true;
}

/**
 * @brief Service the transmit FIFO and status bar updates
 * 
 * This function implements a state machine that:
 * - Drains regular output from tx_fifo to USB/UART
 * - Manages toolbar buffer refresh timing
 * - Ensures toolbar output doesn't corrupt regular output
 * 
 * State machine:
 * - IDLE: Send regular output or prepare toolbar buffer
 * - TOOLBAR_DELAY: Wait for TX FIFO to drain before toolbar
 * - TOOLBAR_TX: Transmit toolbar buffer
 * 
 * @pre Must be called from Core1 only
 * @note Status bar updates are delayed until TX FIFO is empty to prevent VT100 corruption
 */
void tx_fifo_service(void) {
    BP_ASSERT_CORE1(); // tx fifo is drained from core1 only

/** @name TX State Machine States */
/**@{*/
#define IDLE 0             /**< Idle state - send regular output */
#define TOOLBAR_DELAY 1    /**< Waiting for TX FIFO to drain */
#define TOOLBAR_TX 2       /**< Transmitting toolbar buffer */
/**@}*/
    static uint8_t tx_state = IDLE;
    static uint8_t delay_count = 0;

    uint32_t bytes_available;
    char data[64];
    uint8_t i = 0;

    if (system_config.terminal_usb_enable) { // is tinyUSB CDC ready?
        if (tud_cdc_n_write_available(0) < 64) {
            return;
        }
    }

    switch (tx_state) {
        case IDLE:
            bytes_available = spsc_queue_level(&tx_fifo);
            if (bytes_available) {
                i = 0;
                while (spsc_queue_try_remove(&tx_fifo, (uint8_t*)&data[i])) {
                    i++;
                    if (i >= 64) {
                        break;
                    }
                }
                break; // break out of switch and continue below
            }

            // toolbar buffer update is ready
            if (tx_tb_buf_ready) {
                tx_state = TOOLBAR_DELAY;
                tx_tb_buf_index = 0;
                return; // return for next state
            }

            return; // nothing, just return

            break;
        case TOOLBAR_DELAY:
            // Multi-cycle drain: require 3 consecutive empty checks before
            // committing to toolbar TX. This ensures any in-flight bytes from
            // Core0 have time to arrive in the FIFO before we start sending
            // toolbar VT100 sequences that would corrupt interleaved output.
            bytes_available = spsc_queue_level(&tx_fifo);
            if (bytes_available) {
                delay_count = 0;
                tx_state = IDLE; // bytes arrived, go drain them first
            } else {
                delay_count++;
                if (delay_count >= 3) {
                    delay_count = 0;
                    tx_state = TOOLBAR_TX;
                }
            }
            return; // return for next cycle

            break;
        case TOOLBAR_TX: {
            // Send up to 64 bytes directly from tx_tb_buf (no intermediate copy)
            uint16_t remaining = tx_tb_buf_cnt - tx_tb_buf_index;
            uint8_t chunk = (remaining > 64) ? 64 : (uint8_t)remaining;

            // write to terminal usb
            if (system_config.terminal_usb_enable) {
                tud_cdc_n_write(0, &tx_tb_buf[tx_tb_buf_index], chunk);
                tud_cdc_n_write_flush(0);
                if (system_config.terminal_uart_enable) {
                    tud_task();
                }
            }

            // write to terminal debug uart
            if (system_config.terminal_uart_enable) {
                for (uint8_t j = 0; j < chunk; j++) {
                    uart_putc(debug_uart[system_config.terminal_uart_number].uart,
                              tx_tb_buf[tx_tb_buf_index + j]);
                }
            }

            tx_tb_buf_index += chunk;
            if (tx_tb_buf_index >= tx_tb_buf_cnt) {
                tx_tb_buf_ready = false;
                tx_state = IDLE;
            }
            return; // already sent — skip the common write path below
        }
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
    BP_ASSERT_CORE0(); // tx fifo should only be added to from core 0 (deadlock risk)
    spsc_queue_add_blocking(&tx_fifo, (uint8_t)*c);
}

void tx_fifo_try_put(char* c) {
    BP_ASSERT_CORE0(); // tx fifo shoudl only be added to from core 0 (deadlock risk)
    spsc_queue_try_add(&tx_fifo, (uint8_t)*c);
}

void tx_fifo_write(const char* buf, uint32_t len) {
    BP_ASSERT_CORE0();
    for (uint32_t i = 0; i < len; i++) {
        spsc_queue_add_blocking(&tx_fifo, (uint8_t)buf[i]);
    }
}

void tx_fifo_wait_drain(void) {
    BP_ASSERT_CORE0();
    while (!spsc_queue_is_empty(&tx_fifo)) {
        tight_loop_contents();
    }
}

void bin_tx_fifo_put(const char c) {
    BP_ASSERT_CORE0(); // tx fifo shoudl only be added to from core 0 (deadlock risk)
    spsc_queue_add_blocking(&bin_tx_fifo, (uint8_t)c);
}

bool bin_tx_fifo_try_get(char* c) {
    BP_ASSERT_CORE1(); // tx fifo is drained from core1 only
    return spsc_queue_try_remove(&bin_tx_fifo, (uint8_t*)c);
}

void bin_tx_fifo_service(void) {
    BP_ASSERT_CORE1(); // tx fifo is drained from core1 only

    uint32_t bytes_available;
    char data[64];
    uint8_t i = 0;

    // is tinyUSB CDC ready?
    if (tud_cdc_n_write_available(1) < 64) {
        return;
    }

    bytes_available = spsc_queue_level(&bin_tx_fifo);
    if (bytes_available) {
        i = 0;
        while (spsc_queue_try_remove(&bin_tx_fifo, (uint8_t*)&data[i])) {
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
    return !spsc_queue_is_empty(&bin_tx_fifo);
}
