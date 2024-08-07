void la_test_args (struct command_result *res);
bool logicanalyzer_setup(void);
int logicanalyzer_status(void);
uint8_t logicanalyzer_dump(uint8_t *txbuf);
bool logic_analyzer_is_done(void);
bool logic_analyzer_arm(float freq, uint32_t samples, uint32_t trigger_mask, uint32_t trigger_direction, bool edge);
bool logic_analyzer_cleanup(void);
void logicanalyzer_reset_led(void);
void la_periodic(void);