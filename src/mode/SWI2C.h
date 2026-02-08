/**
 * @file SWI2C.h
 * @brief Software I2C mode interface.
 * @details Provides bit-banged I2C protocol mode.
 */

void SWI2C_start(void);
void SWI2C_stop(void);
uint32_t SWI2C_write(uint32_t d);
uint32_t SWI2C_read(void);
void SWI2C_macro(uint32_t macro);
void SWI2C_setup(void);
void SWI2C_setup_exc(void);
void SWI2C_cleanup(void);
void SWI2C_pins(void);
void SWI2C_settings(void);
void SWI2C_help(void);
void I2C_search(void);

#define SWI2CSPEEDMENU "\r\nSpeed\r\n 1. 100kHz\r\n 2. 400Khz\r\nspeed> "

#define LA_SWI2C_PERIOD_100KHZ (((100000000 / 100) / 4) / (10000000 / 72000)) / 10
#define LA_SWI2C_PERIOD_400KHZ (((100000000 / 400) / 4) / (10000000 / 72000)) / 10
