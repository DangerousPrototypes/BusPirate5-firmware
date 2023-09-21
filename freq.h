void freq_cont(struct command_attributes *attributes, struct command_response *response);
void freq_single(struct command_attributes *attributes, struct command_response *response);

uint32_t freq_measure(int32_t pin, int refresh);
uint32_t freq_configure_disable(void);
uint32_t freq_configure_enable(void);

float freq_measure_period(uint gpio);
float freq_measure_duty_cycle(uint gpio);
void freq_display_hz(float* freq_hz_value, float* freq_friendly_value, uint8_t* freq_friendly_units);
void freq_display_ns(float* freq_ns_value, float* period_friendly_value, uint8_t* period_friendly_units);

void freq_measure_period_irq(void);