void ioexp_output_enable(bool enable);
void ioexp_clear_set(uint16_t clear_bits, uint16_t set_bits);
void ioexp_in_out(uint16_t input, uint16_t output);
void ioexp_init(void);
void ioexp_interrupt_enable(uint16_t mask);
bool ioexp_read_bit(uint8_t pin);