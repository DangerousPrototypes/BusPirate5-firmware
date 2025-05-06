void xl9555_init(void);
void xl9555_output_enable(bool enable);
void xl9555_write_wait(uint8_t *level, uint8_t *direction);
void xl9555_interrupt_enable(uint16_t mask);
bool xl9555_read_bit(uint8_t pin);
void xl9555_write_wait_error(uint8_t *level, uint8_t *direction);