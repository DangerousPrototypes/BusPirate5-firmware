bool psucmd_init(void);
void psucmd_enable_handler(struct command_result* res);
uint32_t psucmd_enable(float volts, float current, bool current_limit_override, uint8_t undervoltage_percent);
void psucmd_disable_handler(struct command_result* res);
void psucmd_disable(void);
void psucmd_irq_callback(void);
void psucmd_show_clear_error(void);