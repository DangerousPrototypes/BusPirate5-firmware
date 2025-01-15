int pio_irio_rx_init(uint pin_demod);
void pio_irio_rx_deinit(uint pin_num);
int pio_irio_tx_init(uint pin_num, uint32_t mod_freq);
void pio_irio_tx_deinit(uint pin_num);

void pio_irio_tx_mod_freq(float mod_freq);

void pio_irio_mode_drain_fifo(void);
bool pio_irio_mode_wait_idle(void);

ir_rx_status_t pio_irio_mode_get_frame(uint32_t *rx_frame);
void pio_irio_mode_tx_write(uint32_t *data);

void pio_irio_tx_frame_raw(float mod_freq, uint16_t pairs, uint32_t *buffer);
bool pio_irio_rx_frame_raw(float *mod_freq, uint16_t *pairs, uint32_t *buffer);
