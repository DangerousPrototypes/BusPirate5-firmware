int irio_pio_rx_init(uint pin_demod);
void irio_pio_rx_deinit(uint pin_num);
int irio_pio_tx_init(uint pin_num, uint32_t mod_freq);
void irio_pio_tx_deinit(uint pin_num);

void irio_pio_tx_set_mod_freq(float mod_freq);

void irio_pio_rxtx_drain_fifo(void);
bool irio_pio_tx_wait_idle(void);

ir_rx_status_t irio_pio_rx_frame_printf(uint32_t *rx_frame);
void irio_pio_tx_write(uint32_t *data);

void irio_pio_tx_frame_write(float mod_freq, uint16_t pairs, uint32_t *buffer);
bool irio_pio_rx_frame_buf(float *mod_freq, uint16_t *us, uint16_t *pairs, uint32_t *buffer);

void irio_pio_rx_reset_mod_freq(void);
