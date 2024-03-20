void shift_init(void);
void shift_output_enable(bool enable);
//set and clear bits on shift register, with selectable spinlock (spi_busy_wait)
void shift_clear_set(uint16_t clear_bits, uint16_t set_bits, bool busy_wait);
//set and clear bits on shift registers, spinlock (spi_busy_wait) enabled
void shift_clear_set_wait(uint16_t clear_bits, uint16_t set_bits);

