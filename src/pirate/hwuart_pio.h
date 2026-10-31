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