#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pirate.h"
#include "pirate/hwuart_pio.h"
#include "pirate/bio.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "system_config.h"
#include "binmodes.h"
#include "spsc_queue.h"
#include "tusb.h"
#include "bpio_uart.h"
#include "bpio_reader.h"
#include "bpio_transactions.h"

// Maximum bytes to read in async handler
#define BPIO_MAX_READ_SIZE 512

// Received bytes are moved out of the UART's 32-byte RX FIFO by an interrupt,
// because the BPIO service loop can be blocked for milliseconds at a time:
// send_packet() waits for USB TX space, which core1's tud_task() frees, and
// core1 pauses USB for its periodic voltage-monitor and LCD updates. At 115200
// baud the FIFO fills in under 3 ms. 1024 bytes holds 10 ms at 1 Mbaud, about
// twice the largest backlog measured there.
#define BPIO_UART_RX_BUF_SIZE 1024 // must be a power of 2 (spsc_queue)
static uint8_t rx_buf[BPIO_UART_RX_BUF_SIZE];
static spsc_queue_t rx_queue;

#define BPIO_CDC_INTF 1 // USB CDC interface used by BPIO (CDC_INTF in bpio.c)

static void bpio_hwuart_rx_irq_handler(void) {
    // Received data belongs to BPIO only while it drives UART mode with a host
    // attached, the same condition under which the service loop reads it. When
    // that ends (terminal mode change, binmode switch, host disconnect), disarm
    // without reading so the bytes stay in the FIFO for the terminal's UART
    // functions. The async handler re-arms.
    if (system_config.mode != HWUART || system_config.binmode_select != BINMODE_USE_DIRTYPROTO ||
        !tud_cdc_n_connected(BPIO_CDC_INTF)) {
        uart_set_irqs_enabled(M_UART_PORT, false, false);
        return;
    }
    // Always read, even when the queue is full: the interrupt stays asserted
    // until the FIFO is drained below its trigger level.
    while (uart_is_readable(M_UART_PORT)) {
        spsc_queue_try_add(&rx_queue, (uint8_t)uart_getc(M_UART_PORT));
    }
}

// Arms the RX interrupt if needed and reports whether received data is being
// queued. uart_init() resets the UART, clearing its interrupt mask, on every
// BPIO mode change or reconfigure, so the hardware mask is the armed state.
// Returns false if another handler owns this UART's interrupt (for example the
// terminal on this UART); callers then read the FIFO directly.
static bool bpio_hwuart_rx_irq_arm(void) {
    const uint irq = UART_IRQ_NUM(M_UART_PORT);
    irq_handler_t handler = irq_get_exclusive_handler(irq);
    if (irq_has_shared_handler(irq) || (handler != NULL && handler != bpio_hwuart_rx_irq_handler)) {
        return false;
    }
    if (uart_get_hw(M_UART_PORT)->imsc & UART_UARTIMSC_RXIM_BITS) {
        return true;
    }
    if (handler == NULL) {
        irq_set_exclusive_handler(irq, bpio_hwuart_rx_irq_handler);
    }
    // Anything still queued predates the current UART configuration.
    spsc_queue_init(&rx_queue, rx_buf, sizeof(rx_buf));
    irq_set_enabled(irq, true);
    uart_set_irqs_enabled(M_UART_PORT, true, false);
    return true;
}

// Waits up to timeout_us for a received byte. Returns false on timeout.
static bool bpio_hwuart_getc_within_us(uint8_t *c, uint32_t timeout_us) {
    if (!bpio_hwuart_rx_irq_arm()) {
        if (!uart_is_readable_within_us(M_UART_PORT, timeout_us)) {
            return false;
        }
        *c = (uint8_t)uart_getc(M_UART_PORT);
        return true;
    }
    const absolute_time_t deadline = make_timeout_time_us(timeout_us);
    while (!spsc_queue_try_remove(&rx_queue, c)) {
        if (time_reached(deadline)) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
}

// UART transaction handler for BPIO
// Handles transmit and/or receive operations in response to DataRequest
uint32_t bpio_hwuart_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read) {
    if(request->debug) printf("[UART] Performing transaction\r\n");

    // Transmit bytes if requested
    if(request->bytes_write > 0) {
        if(request->debug) printf("[UART] Writing %d bytes\r\n", request->bytes_write);
        for(uint32_t i = 0; i < request->bytes_write; i++) {
            uart_putc_raw(M_UART_PORT, flatbuffers_uint8_vec_at(data_write, i));
        }
    }

    // Receive bytes if requested
    if(request->bytes_read > 0) {
        if(request->debug) printf("[UART] Reading %d bytes\r\n", request->bytes_read);
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            // Wait for data with timeout (avoid blocking forever)
            if(!bpio_hwuart_getc_within_us(&data_read[i], 1000000)) { // 1 second timeout
                if(request->debug) printf("[UART] Read timeout at byte %d\r\n", i);
                return true; // Error - timeout
            }
        }
    }

    return false; // Success
}

// UART async handler for BPIO
// Checks for unsolicited incoming UART data and reads it into buffer
// Returns number of bytes read (0 if no data available)
//TODO: UART is dispatched to multiple processes!!!
uint32_t bpio_hwuart_async_handler(uint8_t *data_read) {
    if (bpio_hwuart_rx_irq_arm()) {
        uint32_t bytes_read = 0;
        while (bytes_read < BPIO_MAX_READ_SIZE && spsc_queue_try_remove(&rx_queue, &data_read[bytes_read])) {
            bytes_read++;
        }
        return bytes_read;
    }

    // Check if UART has data available
    if(!uart_is_readable(M_UART_PORT)) {
        return 0;
    }
    
    // Read first byte
    data_read[0] = (uint8_t)uart_getc(M_UART_PORT);
    uint32_t bytes_read = 1;
    
    // Wait up to 200us for more data to arrive and batch into single packet
    // Returns immediately if data arrives sooner, improving responsiveness
    // At 115200 baud: ~87us per byte, so typically captures 2-3 bytes
    // At 9600 baud: ~1042us per byte, so may not capture additional bytes
    // Only wait if FIFO empty - then batch whatever arrives
    if(!uart_is_readable(M_UART_PORT)) {
        uart_is_readable_within_us(M_UART_PORT, 200);
    }

    // Drain all available data
    while(uart_is_readable(M_UART_PORT) && bytes_read < BPIO_MAX_READ_SIZE) {
        data_read[bytes_read++] = (uint8_t)uart_getc(M_UART_PORT);
    }
    
    return bytes_read;
}
