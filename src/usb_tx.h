/**
 * @file usb_tx.h
 * @brief USB transmit queue management.
 * @details Provides USB output queue handling for normal and binary modes.
 */

/**
 * @brief Initialize transmit FIFO.
 */
void tx_fifo_init(void);

/**
 * @brief Service transmit FIFO.
 */
void tx_fifo_service(void);

/**
 * @brief Put character in transmit FIFO.
 * @param c  Character to transmit
 */
void tx_fifo_put(char* c);

/**
 * @brief Try to put character in transmit FIFO.
 * @param c  Character to transmit
 */
void tx_fifo_try_put(char* c);

/**
 * @brief Start status bar transmission.
 * @param valid_characters_in_status_bar  Number of valid characters
 */
void tx_sb_start(uint32_t valid_characters_in_status_bar);

/**
 * @brief Put character in binary transmit FIFO.
 * @param c  Character to transmit
 */
void bin_tx_fifo_put(const char c);

/**
 * @brief Service binary transmit FIFO.
 */
void bin_tx_fifo_service(void);

/**
 * @brief Check if binary TX FIFO not empty.
 * @return  true if data pending
 */
bool bin_tx_not_empty(void);

/**
 * @brief Try to get character from binary TX FIFO.
 * @param c  Output character
 * @return   true if character available
 */
bool bin_tx_fifo_try_get(char* c);

extern char tx_sb_buf[1024];