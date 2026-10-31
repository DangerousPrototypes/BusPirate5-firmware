/**
 * @file usb_rx.h
 * @brief USB receive queue management.
 * @details Provides USB and terminal input queue handling for normal and binary modes.
 *          Uses lock-free SPSC queues for thread-safe inter-core communication.
 */

#include "spsc_queue.h"
extern spsc_queue_t rx_fifo;
extern spsc_queue_t bin_rx_fifo;

/**
 * @brief Initialize receive FIFO.
 */
void rx_fifo_init(void);

/**
 * @brief Initialize UART receive IRQ.
 */
void rx_uart_init_irq(void);

/**
 * @brief Initialize USB receive.
 */
void rx_usb_init(void);

/**
 * @brief Get terminal input from RTT.
 */
void rx_from_rtt_terminal(void);

/**
 * @brief Add character to receive FIFO.
 * @param c  Character to add
 */
void rx_fifo_add(char* c);

/**
 * @brief Try to get character from FIFO without blocking.
 * @param c  Output character
 * @return   true if character available
 */
bool rx_fifo_try_get(char* c);

/**
 * @brief Get character from FIFO (blocking).
 * @param c  Output character
 */
void rx_fifo_get_blocking(char* c);

/**
 * @brief Try to peek next character without removing.
 * @param c  Output character
 * @return   true if character available
 */
bool rx_fifo_try_peek(char* c);

/**
 * @brief Peek next character (blocking).
 * @param c  Output character
 */
void rx_fifo_peek_blocking(char* c);

/**
 * @brief Add character to binary receive FIFO.
 * @param c  Character to add
 */
void bin_rx_fifo_add(char* c);

/**
 * @brief Get character from binary FIFO (blocking).
 * @param c  Output character
 */
void bin_rx_fifo_get_blocking(char* c);

/**
 * @brief Get available bytes in binary FIFO.
 * @param cnt  Output byte count
 */
void bin_rx_fifo_available_bytes(uint16_t* cnt);

/**
 * @brief Try to get character from binary FIFO.
 * @param c  Output character
 * @return   true if character available
 */
bool bin_rx_fifo_try_get(char* c);
