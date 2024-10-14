void hwuart_pio_init(uint8_t data_bits, uint8_t parity, uint8_t stop_bits, uint32_t baud);
void hwuart_pio_deinit();
bool hwuart_pio_read(uint32_t* raw, uint8_t* cooked);
void hwuart_pio_write(uint32_t data);