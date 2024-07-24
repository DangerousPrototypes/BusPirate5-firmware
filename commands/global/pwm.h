void pwm_configure_enable(struct command_result *res);
void pwm_configure_disable(struct command_result *res);
uint8_t pwm_freq_find(float* freq_hz_value, float* pwm_hz_actual, float* pwm_ns_actual, uint32_t* pwm_divider, uint32_t* pwm_top);

