// File to abstract MCU/SDK/dev toolchain specific stuff for easier porting in the future

#define mcu_adc_init()   adc_init(); adc_gpio_init(AMUX_OUT); adc_gpio_init(CURRENT_SENSE)
#define mcu_adc_select(X) adc_select_input(X)
#define mcu_adc_read() adc_read()
uint64_t mcu_get_unique_id(void);
void mcu_reset(struct command_attributes *attributes, struct command_response *response);