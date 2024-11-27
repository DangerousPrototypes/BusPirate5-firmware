#define DMA_BYTES_PER_CHUNK 32768
#define LA_DMA_COUNT 4

bool logicanalyzer_setup(void);
int logicanalyzer_status(void);
void logic_analyzer_dump(uint8_t* txbuf);
bool logic_analyzer_is_done(void);
void logic_analyser_done(void);
bool logic_analyzer_configure(
    float freq, uint32_t samples, uint32_t trigger_mask, uint32_t trigger_direction, bool edge, bool interrupt);
void logic_analyzer_arm(bool led_indicator_enable);
bool logic_analyzer_cleanup(void);
void logicanalyzer_reset_led(void);
uint32_t logic_analyzer_get_start_ptr(uint32_t sample_count);
uint32_t logic_analyzer_get_current_ptr(void);
uint32_t logic_analyzer_get_end_ptr(void);
void logic_analyzer_reset_ptr(void);
uint8_t logic_analyzer_read_ptr(uint32_t read_pointer);
void logic_analyzer_set_base_pin(uint8_t base_pin);
uint32_t logic_analyzer_get_samples_from_zero(void);