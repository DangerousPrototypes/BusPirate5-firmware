/**
 * @file LCDSPI.h
 * @brief LCD SPI mode interface.
 * @details Provides SPI-based LCD driver mode supporting HD44780 and ST7735.
 */

uint32_t LCDSPI_send(uint32_t d);
uint32_t LCDSPI_read(void);
void LCDSPI_macro(uint32_t macro);
void LCDSPI_setup(void);
void LCDSPI_setup_exc(void);
void LCDSPI_cleanup(void);
void LCDSPI_pins(void);
void LCDSPI_settings(void);

enum {
    HD44780 = 0,
#ifdef DISPLAY_USE_ST7735
    ST7735,
#endif
    MAXDISPLAYS
};
