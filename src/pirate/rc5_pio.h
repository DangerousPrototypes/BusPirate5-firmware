int rc5_rx_init(uint pin_num);
int rc5_tx_init(uint pin_num);
void rc5_rx_deinit(uint pin_num);
void rc5_tx_deinit(uint pin_num);
void rc5_send(uint32_t *data);
nec_rx_status_t rc5_receive(uint32_t *rx_frame);
void rc5_test(void);
bool rc5_tx_wait_idle(void);
void rc5_drain_fifo(void);