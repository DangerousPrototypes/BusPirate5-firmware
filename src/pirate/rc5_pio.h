/**
 * @file rc5_pio.h
 * @brief RC5 infrared protocol PIO interface.
 * @details Provides RC5 IR transmit/receive using PIO state machines.
 */

/**
 * @brief Initialize RC5 receiver.
 * @param pin_num  GPIO pin for receiver
 * @return         PIO offset or error code
 */
int rc5_rx_init(uint pin_num);

/**
 * @brief Initialize RC5 transmitter.
 * @param pin_num   GPIO pin for transmitter
 * @param mod_freq  Modulation frequency in Hz
 * @return          PIO offset or error code
 */
int rc5_tx_init(uint pin_num, uint32_t mod_freq);

/**
 * @brief Deinitialize RC5 receiver.
 * @param pin_num  GPIO pin
 */
void rc5_rx_deinit(uint pin_num);

/**
 * @brief Deinitialize RC5 transmitter.
 * @param pin_num  GPIO pin
 */
void rc5_tx_deinit(uint pin_num);

/**
 * @brief Send RC5 frame.
 * @param data  Frame data
 */
void rc5_send(uint32_t *data);

/**
 * @brief Receive RC5 frame.
 * @param rx_frame  Output frame data
 * @return          Reception status
 */
ir_rx_status_t rc5_receive(uint32_t *rx_frame);

/**
 * @brief Test RC5 functionality.
 */
void rc5_test(void);

/**
 * @brief Wait for RC5 transmit to complete.
 * @return  true when idle
 */
bool rc5_tx_wait_idle(void);

/**
 * @brief Drain RC5 FIFO.
 */
void rc5_drain_fifo(void);