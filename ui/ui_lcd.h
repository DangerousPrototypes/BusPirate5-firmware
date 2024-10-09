void lcd_configure(void);
void lcd_clear(void);
void ui_lcd_update(uint32_t update_flags);
void lcd_write_background(const unsigned char *image);
void lcd_screensaver_disable(void);
void lcd_screensaver_enable(void);
void lcd_set_bounding_box(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye);
void lcd_write_command(uint8_t command);
void lcd_write_data(uint8_t data);
void lcd_enable(void);
void lcd_disable(void);
void menu_update(uint8_t current, uint8_t next);


extern const uint8_t colors_pallet[][2];
// Setup the text and background pixel colors
static enum lcd_colors
{
    LCD_RED,
    LCD_ORANGE,
    LCD_YELLOW,
    LCD_GREEN,
    LCD_BLUE,
    LCD_PURPLE,
    LCD_BROWN,
    LCD_GREY,
    LCD_WHITE,
    LCD_BLACK,
} lcd_colors;
