void ch32vswio_reset(int pin, int dirpin);
uint32_t ch32vswio_get(uint32_t addr);
void ch32vswio_put(uint32_t addr, uint32_t data);
void ch32vswio_cleanup(void);