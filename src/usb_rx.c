/**
 * @file usb_rx.c
 * @brief USB, UART, and RTT receive buffer management
 * 
 * This file implements the receive side of the user interface buffering system.
 * It handles input from multiple sources:
 * - USB CDC (normal mode)
 * - UART (debug mode)
 * - SEGGER RTT (Real Time Transfer)
 * 
 * All input sources are funneled into ring buffers that are consumed by
 * the command processor on Core0.
 * 
 * Thread safety: Uses lock-free SPSC queues
 * - Producer: Core1 (USB/UART/RTT handlers)
 * - Consumer: Core0 (command processing)
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "spsc_queue.h"
#include "hardware/dma.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "tusb.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "debug_uart.h"

void rx_uart_irq_handler(void);

/** @defgroup rx_buffers RX Buffer Definitions
 * @brief Receive ring buffers for multiple input sources
 * @{
 */

spsc_queue_t rx_fifo;     /**< Main receive FIFO for terminal input */
spsc_queue_t bin_rx_fifo; /**< Binary mode receive FIFO */

/** RX FIFO size in bits (2^7 = 128 bytes) - TinyUSB requires power of 2 */
#define RX_FIFO_LENGTH_IN_BITS 7
/** RX FIFO size in bytes */
#define RX_FIFO_LENGTH_IN_BYTES (0x0001 << RX_FIFO_LENGTH_IN_BITS)

uint8_t rx_buf[RX_FIFO_LENGTH_IN_BYTES];     /**< RX buffer storage */
uint8_t bin_rx_buf[RX_FIFO_LENGTH_IN_BYTES]; /**< Binary RX buffer storage */

/** @} */ // end of rx_buffers

/**
 * @brief Initialize receive FIFOs
 * 
 * Sets up the lock-free SPSC ring buffers for regular and binary mode reception.
 * Buffer sizes must be powers of 2 for efficient modulo operation.
 * 
 * @note Can be called from either core
 */
void rx_fifo_init(void) {
    // OK to call from either core
    spsc_queue_init(&rx_fifo, rx_buf, RX_FIFO_LENGTH_IN_BYTES);
    spsc_queue_init(&bin_rx_fifo, bin_rx_buf, RX_FIFO_LENGTH_IN_BYTES);
}

/**
 * @brief Enable UART receive interrupt for debug mode
 * 
 * Configures the UART interrupt handler for receiving debug terminal input.
 * The interrupt is set as exclusive to Core1 to prevent deadlocks.
 * 
 * @pre Debug UART must already be configured (see debug_uart.c)
 * @pre Must be called from Core1
 * 
 * @warning Calling from any core other than Core1 will trigger assertion
 */
void rx_uart_init_irq(void) {
    // RX FIFO (whether from UART, CDC, RTT, ...) should only be added to from core1 (deadlock risk)
    // irq_set_exclusive_handler() sets the interrupt to be exclusive to the current core.
    // therefore, this function must be called from Core1.
    BP_ASSERT_CORE1();
    // rx interrupt
    irq_set_exclusive_handler(debug_uart[system_config.terminal_uart_number].irq, rx_uart_irq_handler);
    // irq_set_priority(debug_uart[system_config.terminal_uart_number].irq, 0xFF);
    irq_set_enabled(debug_uart[system_config.terminal_uart_number].irq, true);
    uart_set_irq_enables(debug_uart[system_config.terminal_uart_number].uart, true, false);
}

/**
 * @brief UART receive interrupt handler
 * 
 * Reads all available bytes from the UART RX FIFO and pushes them into
 * the rx_fifo buffer for processing by the command interpreter.
 * 
 * @pre Must execute on Core1 only
 * @note Uses non-blocking add - drops characters if queue is full (better than deadlock)
 */
void rx_uart_irq_handler(void) {
    BP_ASSERT_CORE1(); // RX FIFO (whether from UART, CDC, RTT, ...) should only be added to from core1 (deadlock risk)
    // while bytes available shove them in the buffer
    while (uart_is_readable(debug_uart[system_config.terminal_uart_number].uart)) {
        uint8_t c = uart_getc(debug_uart[system_config.terminal_uart_number].uart);
        // Use try_add instead of blocking - better to drop chars than deadlock in ISR
        spsc_queue_try_add(&rx_fifo, c);
    }
}

/**
 * @brief Initialize USB subsystem
 * 
 * Initializes the TinyUSB stack for USB CDC communication.
 * 
 * @pre Must be called from Core1
 */
void rx_usb_init(void) {
    BP_ASSERT_CORE1();
    tusb_init();
}

/**
 * @brief Read data from SEGGER RTT and push to RX queue
 * 
 * This function polls RTT for input characters and adds them to the rx_fifo.
 * If the queue is full, the last character is stored in a static variable
 * and retried on the next call, ensuring no characters are lost.
 * 
 * @pre Must be called from Core1 only
 * 
 * @note Uses static variable to preserve characters when queue is full
 * @note Non-blocking - returns immediately if no data or queue is full
 */
void rx_from_rtt_terminal(void) {
    BP_ASSERT_CORE1(); // RX FIFO (whether from UART, CDC, RTT, ...) should only be added to from core1 (deadlock risk)

    static int last_character = -1;

    // if prior callback left a prior character, use it first.
    int current_character = (last_character >= 0) ? last_character : SEGGER_RTT_GetKey();
    last_character = -1;

    // Still might not be any characters available, but if one was obtained...
    while (current_character >= 0) {
        // Try to add it to the RX FIFO...
        if (!spsc_queue_try_add(&rx_fifo, (uint8_t)current_character)) {
            // and if that fails, store the character for the next time
            // this function is called
            last_character = current_character;
            break; // out of the while() loop b/c out of RX FIFO buffer
        }
        // Else, there was a character, and it got added to the RX FIFO.
        // So, try to get another character....
        current_character = SEGGER_RTT_GetKey();
    }
}



// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_TEMP,
        "--> tud_cdc_line_state_cb(%d, %d, %d)\n", itf, dtr, rts
        );
    system_config.rts = rts;
}

// insert a byte into the queue
void rx_fifo_add(char* c) {
    BP_ASSERT_CORE1(); // RX FIFO (whether from UART, CDC, RTT, ...) should only be added to from core1 (deadlock risk)
    spsc_queue_add_blocking(&rx_fifo, (uint8_t)*c);
}

// functions to access the ring buffer from other code
// block until a byte is available, remove from buffer
void rx_fifo_get_blocking(char* c) {
    BP_ASSERT_CORE0(); // RX FIFO (whether from UART, CDC, RTT, ...) should only be drained from core0 (deadlock risk)
    spsc_queue_remove_blocking(&rx_fifo, (uint8_t*)c);
}
// try to get a byte, remove from buffer if available, return false if no byte
bool rx_fifo_try_get(char* c) {
    // OK to call from either core
    return spsc_queue_try_remove(&rx_fifo, (uint8_t*)c);
}
// block until a byte is available, return byte but leave in buffer
void rx_fifo_peek_blocking(char* c) {
    BP_ASSERT_CORE0(); // RX FIFO (whether from UART, CDC, RTT, ...) should only be drained from core0 (deadlock risk)
    spsc_queue_peek_blocking(&rx_fifo, (uint8_t*)c);
}
// try to peek at next byte, return byte but leave in buffer, return false if no byte
bool rx_fifo_try_peek(char* c) {
    // OK to call from either core
    return spsc_queue_try_peek(&rx_fifo, (uint8_t*)c);
}

// BINMODE queue
void bin_rx_fifo_add(char* c) {
    BP_ASSERT_CORE1();
    spsc_queue_add_blocking(&bin_rx_fifo, (uint8_t)*c);
}

void bin_rx_fifo_get_blocking(char* c) {
    BP_ASSERT_CORE0(); // RX FIFO (whether from UART, CDC, RTT, ...) should only be drained from core0 (deadlock risk)
    spsc_queue_remove_blocking(&bin_rx_fifo, (uint8_t*)c);
}

void bin_rx_fifo_available_bytes(uint16_t* cnt) {
    // OK to call from either core
    *cnt = (uint16_t)spsc_queue_level(&bin_rx_fifo);
}

bool bin_rx_fifo_try_get(char* c) {
    // OK to call from either core
    bool result = spsc_queue_try_remove(&bin_rx_fifo, (uint8_t*)c);
    return result;
}