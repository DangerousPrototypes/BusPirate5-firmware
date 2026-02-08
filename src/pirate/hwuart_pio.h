/**
 * @file hwuart_pio.h
 * @brief UART protocol implementation using PIO.
 * @details PIO-based UART with configurable data bits, parity, and stop bits.
 *          Uses separate state machines for TX and RX for full-duplex operation.
 */

/**
 * @brief Initialize UART PIO state machines.
 * @param data_bits  Data width (5-8 bits)
 * @param parity     Parity mode (UART_PARITY_NONE, EVEN, or ODD)
 * @param stop_bits  Stop bits (1 or 2)
 * @param baud       Baud rate in bits per second
 */
void hwuart_pio_init(uint8_t data_bits, uint8_t parity, uint8_t stop_bits, uint32_t baud);

/**
 * @brief Deinitialize and remove UART PIO programs.
 */
void hwuart_pio_deinit();

/**
 * @brief Read received UART data if available.
 * @param[out] raw     Raw data word from FIFO
 * @param[out] cooked  Processed data byte
 * @return true if data was read, false if RX FIFO empty
 */
bool hwuart_pio_read(uint32_t* raw, uint8_t* cooked);

/**
 * @brief Transmit data on UART.
 * @param data  Data word to transmit
 */
void hwuart_pio_write(uint32_t data);