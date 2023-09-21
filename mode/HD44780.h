

void HD44780_setup(void);
void HD44780_cleanup(void);
uint32_t HD44780_write(uint32_t d);
uint32_t HD44780_read(void);
void HD44780_writenibble(uint8_t rs, uint8_t d);
void HD44780_init(uint8_t lines);
void HD44780_reset(void);
void HD44780_macro(uint32_t macro);


