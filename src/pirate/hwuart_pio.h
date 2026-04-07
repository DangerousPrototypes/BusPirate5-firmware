/**
 * @file hwuart_pio.h
 * @brief UART protocol implementation using PIO.
 * @details PIO-based UART with configurable data bits, parity, and stop bits.
 *          Uses separate state machines for TX and RX for full-duplex operation.
 */

struct hduart_mode_config;

/**
 * @brief Initialize UART PIO state machines.
 * @param cfg  Pointer to half-duplex UART mode configuration
 */
void hwuart_pio_init(const struct hduart_mode_config *cfg);

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

/**
 * @brief Initialize PIO UART in full-duplex mode with separate TX and RX pins.
 * @param tx_pin    BIO pin number for TX (0-7)
 * @param rx_pin    BIO pin number for RX (0-7)
 * @param data_bits Number of data bits (5-8)
 * @param stop_bits Number of stop bits (1 or 2)
 * @param parity    Parity mode (UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD)
 * @param baudrate  Baud rate in bits per second
 */
void hwuart_pio_fullduplex_init(uint8_t tx_pin, uint8_t rx_pin,
                                uint8_t data_bits, uint8_t stop_bits,
                                uint8_t parity, uint32_t baudrate);

/**
 * @brief Transmit data in full-duplex PIO mode (no half-duplex pin toggling).
 * @param data  Data word to transmit
 */
void hwuart_pio_fullduplex_write(uint32_t data);

/**
 * @brief Read received data in full-duplex PIO mode.
 * @param[out] raw     Raw data word from FIFO
 * @param[out] cooked  Processed data byte
 * @return true if data was read, false if RX FIFO empty
 */
bool hwuart_pio_fullduplex_read(uint32_t* raw, uint8_t* cooked);

/**
 * @brief Discard any pending data in the full-duplex RX FIFO.
 */
void hwuart_pio_fullduplex_flush_rx(void);
