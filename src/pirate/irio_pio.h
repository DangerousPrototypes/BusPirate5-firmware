/**
 * @file irio_pio.h
 * @brief Generic infrared I/O PIO interface.
 * @details Provides configurable IR transmit/receive using PIO state machines.
 */

/**
 * @brief Initialize IR receiver PIO.
 * @param pin_demod  Demodulator input pin
 * @return           PIO offset or error code
 */
int irio_pio_rx_init(uint pin_demod);

/**
 * @brief Deinitialize IR receiver.
 * @param pin_num  GPIO pin
 */
void irio_pio_rx_deinit(uint pin_num);

/**
 * @brief Initialize IR transmitter PIO.
 * @param pin_num   GPIO pin for IR LED
 * @param mod_freq  Modulation frequency in Hz
 * @return          PIO offset or error code
 */
int irio_pio_tx_init(uint pin_num, uint32_t mod_freq);

/**
 * @brief Deinitialize IR transmitter.
 * @param pin_num  GPIO pin
 */
void irio_pio_tx_deinit(uint pin_num);

/**
 * @brief Set IR transmitter modulation frequency.
 * @param mod_freq  Modulation frequency in Hz
 */
void irio_pio_tx_set_mod_freq(float mod_freq);

/**
 * @brief Drain TX/RX FIFOs.
 */
void irio_pio_rxtx_drain_fifo(void);

/**
 * @brief Wait for IR transmit to complete.
 * @return  true when idle
 */
bool irio_pio_tx_wait_idle(void);

/**
 * @brief Receive and print IR frame.
 * @param rx_frame  Output frame data
 * @return          Reception status
 */
ir_rx_status_t irio_pio_rx_frame_printf(uint32_t *rx_frame);

/**
 * @brief Write data to IR transmitter.
 * @param data  Frame data
 */
void irio_pio_tx_write(uint32_t *data);

/**
 * @brief Write complete IR frame with modulation.
 * @param mod_freq  Modulation frequency in Hz
 * @param pairs     Number of mark/space pairs
 * @param buffer    Frame data buffer
 */
void irio_pio_tx_frame_write(float mod_freq, uint16_t pairs, uint32_t *buffer);

/**
 * @brief Receive IR frame into buffer.
 * @param mod_freq  Output modulation frequency
 * @param us        Output microseconds
 * @param pairs     Output number of pairs
 * @param buffer    Frame data buffer
 * @return          true on success
 */
bool irio_pio_rx_frame_buf(float *mod_freq, uint16_t *us, uint16_t *pairs, uint32_t *buffer);

/**
 * @brief Reset receiver modulation frequency detection.
 */
void irio_pio_rx_reset_mod_freq(void);
