void hwuart_pio_init(uint32_t baud);
void hwuart_pio_deinit();
bool hwuart_pio_read(uint32_t *raw, uint8_t *cooked);