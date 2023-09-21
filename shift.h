#define shift_output_enable()       gpio_put(SHIFT_EN, 0); //active low, enable shift register outputs
#define shift_output_disable()      gpio_put(SHIFT_EN, 1); //active low, enable shift register outputs
void shift_init(void);
//set and clear bits on shift register, with selectable spinlock (spi_busy_wait)
void shift_set_clear(uint16_t set_bits, uint16_t clear_bits, bool busy_wait);
//set and clear bits on shift registers, spinlock (spi_busy_wait) enabled
void shift_set_clear_wait(uint16_t set_bits, uint16_t clear_bits);
//select an channel on the analog mux (connected to the shift registers)
void shift_adc_select(uint8_t channel);
