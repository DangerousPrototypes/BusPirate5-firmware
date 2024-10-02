#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "font/font.h"
#include "font/hunter-23pt-24h24w.h"
#include "font/hunter-20pt-21h21w.h"
#include "font/hunter-14pt-19h15w.h"
#include "font/hunter-12pt-16h13w.h"
#include "display/background.h"
#include "display/background_image_v4.h"
#include "ui/ui_lcd.h"
#include "ui/ui_flags.h"
#include "system_monitor.h"
#include "opt_args.h"
#include "commands.h"
#include "bytecode.h"
#include "modes.h"
#include "displays.h"
#include "pirate/lcd.h"

void lcd_write_string(const FONT_INFO *font, const uint8_t *back_color, const uint8_t *text_color, const char *c, uint16_t fill_length);
void lcd_write_labels(uint16_t left_margin, uint16_t top_margin, const FONT_INFO *font, const uint8_t *color, const char* c,uint16_t fill_length);

const uint8_t colors_pallet[][2]=
{
    [LCD_RED]={(uint8_t)(BP_LCD_COLOR_RED>>8),(uint8_t)BP_LCD_COLOR_RED},
    [LCD_ORANGE]={(uint8_t)(BP_LCD_COLOR_ORANGE>>8),(uint8_t)BP_LCD_COLOR_ORANGE},
    [LCD_YELLOW]={(uint8_t)(BP_LCD_COLOR_YELLOW>>8),(uint8_t)BP_LCD_COLOR_YELLOW},
    [LCD_GREEN]={(uint8_t)(BP_LCD_COLOR_GREEN>>8),(uint8_t)BP_LCD_COLOR_GREEN},
    [LCD_BLUE]={(uint8_t)(BP_LCD_COLOR_BLUE>>8),(uint8_t)BP_LCD_COLOR_BLUE},
    [LCD_PURPLE]={(uint8_t)(BP_LCD_COLOR_PURPLE>>8),(uint8_t)BP_LCD_COLOR_PURPLE},
    [LCD_BROWN]={(uint8_t)(BP_LCD_COLOR_BROWN>>8),(uint8_t)BP_LCD_COLOR_BROWN},
    [LCD_GREY]={(uint8_t)(BP_LCD_COLOR_GREY>>8),(uint8_t)BP_LCD_COLOR_GREY},
    [LCD_WHITE]={(uint8_t)(BP_LCD_COLOR_WHITE>>8),(uint8_t)BP_LCD_COLOR_WHITE},
    [LCD_BLACK]={(uint8_t)(BP_LCD_COLOR_BLACK>>8),(uint8_t)BP_LCD_COLOR_BLACK},
};

void menu_update(uint8_t current, uint8_t next)
{
    const char b[2]={' ',0x00};
    lcd_set_bounding_box(0,240-1,current*32,(current*32) + hunter_14ptFontInfo.lookup[b[0]-hunter_14ptFontInfo.start_char].height -1 );
    spi_busy_wait(true);    
    gpio_put(DISPLAY_DP, 1); 
    gpio_put(DISPLAY_CS, 0); 
    lcd_write_string(&hunter_14ptFontInfo, colors_pallet[LCD_BLACK], colors_pallet[LCD_RED], b, 0);
    gpio_put(DISPLAY_CS, 1); 
    spi_busy_wait(false);

    const char c[2]={'>',0x00};
    lcd_set_bounding_box(0,240-1,next*32,(next*32) + hunter_14ptFontInfo.lookup[c[0]-hunter_14ptFontInfo.start_char].height -1 );
    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 1); 
    gpio_put(DISPLAY_CS, 0); 
    lcd_write_string(&hunter_14ptFontInfo, colors_pallet[LCD_BLACK], colors_pallet[LCD_RED], c, 0);
    gpio_put(DISPLAY_CS, 1);  
    spi_busy_wait(false);
}



struct display_layout {
    const struct lcd_background_image_info *image;
    const FONT_INFO *font_default;
    const FONT_INFO *font_big;
    uint8_t current_top_pad;
    uint8_t current_left_pad;
    enum lcd_colors current_color;
    uint8_t ma_top_pad;
    uint8_t ma_left_pad;
    enum lcd_colors ma_color;
    uint8_t vout_top_pad;
    uint8_t io_col_top_pad;
    uint8_t io_col_left_pad;    //column begins 7 pixels from the left
    uint8_t io_col_width_chars; //4 character columns
    uint8_t io_col_right_pad; //6 pixels padding space between columns    
    uint8_t io_row_height;
    enum lcd_colors io_name_color;
    enum lcd_colors io_label_color;
    enum lcd_colors io_value_color;
};

const struct display_layout layout={
    &lcd_back_v4,
    &hunter_14ptFontInfo, //const FONT_INFO *font_default;
    &hunter_23ptFontInfo, //const FONT_INFO *font_big;
    36,//uint8_t current_top_pad;
    58, //uint8_t current_left_pad;
    LCD_RED,//enum lcd_colors current_color;
    43,//uint8_t ma_top_pad;
    172,//uint8_t ma_left_pad;
    LCD_RED,//enum lcd_colors ma_color;
    7,//uint8_t vout_top_pad;
    72,//uint8_t io_col_top_pad;
    7,//uint8_t io_col_left_pad;    //column begins 7 pixels from the left
    4,//uint8_t io_col_width_chars; //4 character columns
    6,//uint8_t io_col_right_pad; //6 pixels padding space between columns    
    28,//uint8_t io_row_height;
    LCD_RED,//enum lcd_colors io_name_color;
    LCD_WHITE,//enum lcd_colors io_label_color;
    LCD_RED//enum lcd_colors io_value_color;    
};

void lcd_write_background(const char *image)
{
    uint8_t offset=image[0x0a];
    uint8_t pixel[2];
    uint16_t x,y;
    lcd_set_bounding_box(0, 240, 0, 320);

    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 1);
    gpio_put(DISPLAY_CS, 0);    

    //Update October 2024: new image headers in pre-sorted pixel format for speed
    // see image.py in the display folder to create new headers
    //TODO:pre-adjust the images so we can just DMA it.
    //for(uint32_t b=offset; b<(320*240*2+offset); b+=2){
        //uint8_t *buff = (uint8_t *) &lcd_background_image[b];
        //pixel[0]=image[b+1];
        //pixel[1]=image[b];
        spi_write_blocking(BP_SPI_PORT, image, (320*240*2));  
 
    //}
    
    gpio_put(DISPLAY_CS, 1);
    spi_busy_wait(false);
}

// Write a string to the LCD
//TODO: in LCD write string, automaticall toupper/lower depending on the contents of the font and the string
void lcd_write_string(const FONT_INFO *font, const uint8_t *back_color, const uint8_t *text_color, const char *c, uint16_t fill_length)
{
    uint16_t row;
    uint16_t length=0;
    uint8_t adjusted_c;

    while(*c>0)
    { 
        adjusted_c=(*c)-(*font).start_char;
        for(uint16_t col=0;col<(*font).lookup[adjusted_c].width;col++)
        {
            row=0;
            uint16_t rows = (*font).lookup[adjusted_c].height;
            uint16_t offset=(*font).lookup[adjusted_c].offset;

            for(uint16_t page=0;page<(*font).height_bytes;page++)
            {

                uint8_t bitmap_char = (*font).bitmaps[offset+(col*(*font).height_bytes)+page];              

                for(uint8_t i=0; i<8; i++)
                {
                    if(bitmap_char & (0b10000000>>i))
                    {
                        spi_write_blocking(BP_SPI_PORT, text_color, 2);
                    }
                    else
                    {
                        spi_write_blocking(BP_SPI_PORT, back_color, 2);
                    }
                    
                    row++;
                    //break out of loop when we have all the rows
                    //some bits may be discarded because of poor packing by The Dot Factory
                    if(row==rows) 
                    {
                        break;
                    }
                }

            }
        }
        //depending on how the font fits in the bitmap, 
        //there may or may not be enough right hand padding between characters
        //this adds a configurable amount of space
        for(uint8_t pad=0;pad<(*font).lookup[adjusted_c].height*(*font).right_padding; pad++)
        {
            spi_write_blocking(BP_SPI_PORT, back_color, 2);
        }
        (c)++; 
        length++; //how many characters have we written
    }

    // add additional blank spaces to clear old characters off the line
    if(length<fill_length)
    {
        uint32_t fill=(fill_length-length) * ((*font).lookup[adjusted_c].height * ((*font).right_padding+(*font).lookup[adjusted_c].width));
        for(uint32_t i=0;i<fill; i++)
        {
            spi_write_blocking(BP_SPI_PORT, back_color, 2);
        }        
    }
 
}

uint16_t lcd_get_col(const struct display_layout *layout, const FONT_INFO *font, uint8_t col)
{
    uint16_t col_start=(*font).lookup[0].width + (*font).right_padding; //width of a character
    col_start *= layout->io_col_width_chars;
    col_start += layout->io_col_right_pad;
    col_start *= (col-1);
    col_start += layout->io_col_left_pad;
    return col_start;
}

void lcd_paint_background(void)
{
    uint16_t top_margin;

    // Current measurement
    uint8_t current_string[]="000.0";    
    lcd_write_labels(layout.current_left_pad, layout.current_top_pad, layout.font_big, colors_pallet[layout.current_color], current_string, 0);

    uint8_t ma_string[]="MA";
    lcd_write_labels(layout.ma_left_pad, layout.ma_top_pad, layout.font_default, colors_pallet[layout.ma_color], ma_string, 0);
    
    uint8_t value_string[]="0.0V";
    uint16_t left_margin = lcd_get_col(&layout, layout.font_default, 3); //put us in the third column    
    // Vout voltage
    lcd_write_labels(left_margin, layout.vout_top_pad, layout.font_default, colors_pallet[layout.io_value_color], value_string, 0);
    // IO voltage
    top_margin=layout.io_col_top_pad;    //first line of IO pins
    for(int i=1;i<HW_PINS-1;i++)
    {
        lcd_write_labels(left_margin, top_margin, layout.font_default, colors_pallet[layout.io_value_color], value_string, 0);
        top_margin+=layout.io_row_height;
    }
}

void ui_lcd_update(uint32_t update_flags)
{
    if(update_flags & UI_UPDATE_IMAGE)
    {
        lcd_write_background(layout.image->bitmap);
        lcd_paint_background();
    }
    
    if(update_flags & UI_UPDATE_NAMES) //names
    {
        uint16_t left_margin = lcd_get_col(&layout, layout.font_default, 1); //put us in the first column 
        // IO pin name loop
        uint16_t top_margin=layout.vout_top_pad;       
        for(int i=0;i<HW_PINS;i++){
            lcd_write_labels(left_margin, top_margin, layout.font_default, colors_pallet[layout.io_name_color], hw_pin_label_ordered[i], layout.io_col_width_chars);
            //Vout gets to set own position, the next pins go in even rows
            top_margin = layout.io_col_top_pad + (layout.io_row_height*i);
        }
    }


    if(update_flags & UI_UPDATE_LABELS) //labels
    {
        uint16_t left_margin = lcd_get_col(&layout, layout.font_default, 2); //put us in the second column 
        // IO pin label loop
        uint16_t top_margin=layout.vout_top_pad;   
        for(int i=0;i<HW_PINS;i++)
        {
            if(system_config.pin_changed & (1u<<i) || update_flags&UI_UPDATE_FORCE)
            {
                lcd_write_labels(left_margin, top_margin, layout.font_default, colors_pallet[layout.io_label_color], system_config.pin_labels[i]==0?"-":(char*)system_config.pin_labels[i], layout.io_col_width_chars);
            }
            //Vout gets to set own position, the next pins go in even rows
            top_margin = layout.io_col_top_pad + (layout.io_row_height*i);
        }
    }

    if(update_flags & UI_UPDATE_VOLTAGES)
    {
        uint16_t left_margin = lcd_get_col(&layout, layout.font_default, 3); //put us in the third column 
        uint8_t font_width = layout.font_default->lookup[0].width + layout.font_default->right_padding;
        uint16_t left_margin_skip_two_chars = (font_width * 2)+left_margin;
        char c[]="0";

        // IO pin voltage loop
        uint16_t top_margin=layout.vout_top_pad;
        for(int i=0;i<HW_PINS-1;i++)
        {
            if(monitor_get_voltage_char(i, 0, c) || update_flags&UI_UPDATE_FORCE)
            {
                lcd_write_labels(left_margin, top_margin, layout.font_default, colors_pallet[layout.io_value_color], c, 0);

            }

            if(monitor_get_voltage_char(i, 2, c) || update_flags&UI_UPDATE_FORCE)
            {
                lcd_write_labels(left_margin_skip_two_chars, top_margin, layout.font_default, colors_pallet[layout.io_value_color], c, 0);
            }
            
            //Vout gets to set own position, the next pins go in even rows
            top_margin = layout.io_col_top_pad + (layout.io_row_height*i);
        }
    }

    //reset to 000.0 when psu is disabled
    if(((system_config.pin_changed&(1u<<BP_VOUT))||update_flags&UI_UPDATE_FORCE)  && (system_config.pin_func[BP_VOUT]==BP_PIN_VREF))
    {
        char current_string[]="000.0";    
        lcd_write_labels(layout.current_left_pad, layout.current_top_pad, layout.font_big, colors_pallet[layout.current_color], current_string, 0);
    }
    else if(update_flags & UI_UPDATE_CURRENT || update_flags&UI_UPDATE_FORCE)
    {
        // this is a bit of a hack, the big font is variable width, and the first char (.) is smaller than a number
        // so we go for 3, but thats not really the best way to deal
        uint8_t font_width = layout.font_big->lookup[3].width + layout.font_big->right_padding;
        uint16_t left_margin = layout.current_left_pad;
        char c[]="0";

        //integers
        for(int i=0; i<3; i++)
        {
            if(monitor_get_current_char(i, c) || update_flags&UI_UPDATE_FORCE)
            {
                lcd_write_labels(left_margin, layout.current_top_pad, layout.font_big, colors_pallet[layout.current_color], c, 0);
            }
            left_margin += font_width;
        }
        //decimal
        if(monitor_get_current_char(4, c) || update_flags&UI_UPDATE_FORCE)
        {
            //variable width font, decimal point (0) is smaller
            left_margin +=(layout.font_big->lookup[0].width+layout.font_big->right_padding);
            lcd_write_labels(left_margin, layout.current_top_pad, layout.font_big, colors_pallet[layout.current_color], c, 0);
        }
    }
}

void lcd_write_labels(uint16_t left_margin, uint16_t top_margin, const FONT_INFO *font, const uint8_t *color, const char* c, uint16_t fill_length)
{
    lcd_set_bounding_box(left_margin,left_margin+((240)-1),top_margin,(top_margin + (*font).lookup[(*c)-(*font).start_char].height)-1 );
    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 1); 
    gpio_put(DISPLAY_CS, 0); 
    lcd_write_string(font, layout.image->text_background_color, color, c, fill_length);
    gpio_put(DISPLAY_CS, 1); 
    spi_busy_wait(false);

}

void lcd_clear(void){
    uint16_t x,y;

    lcd_set_bounding_box(0, 240, 0, 320);
    
    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 1);
    gpio_put(DISPLAY_CS, 0); 
    for(x=0;x<240;x++){
        for(y=0;y<320;y++){
            spi_write_blocking(BP_SPI_PORT, colors_pallet[LCD_BLACK], 2);
        }
    }
    gpio_put(DISPLAY_CS, 1);
    spi_busy_wait(false);
}

void lcd_set_bounding_box(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye){
    //setup write area
    //start must always be =< end
    lcd_write_command(0x2A); //column start and end set
    lcd_write_data(ys>>8);
    lcd_write_data(ys&0xff); //0
    lcd_write_data(ye>>8);
    lcd_write_data(ye&0xff); //320

    lcd_write_command(0x2B); //row start and end set

    lcd_write_data(xs>>8);
    lcd_write_data(xs&0xff); //0
    lcd_write_data(xe>>8);
    lcd_write_data(xe&0xff); //LCD_W (240)
    lcd_write_command(0x2C);//Memory Write
}

void lcd_write_command(uint8_t command){
    //D/C low for command
    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 0); //gpio_clear(BP_LCD_DP_PORT,BP_LCD_DP_PIN);
    gpio_put(DISPLAY_CS, 0); //gpio_clear(BP_LCD_CS_PORT, BP_LCD_CS_PIN);
    spi_write_blocking(BP_SPI_PORT, &command, 1); //spi_xfer(BP_LCD_SPI, (uint16_t) command);
    gpio_put(DISPLAY_CS, 1); //gpio_set(BP_LCD_CS_PORT, BP_LCD_CS_PIN);
    spi_busy_wait(false);
}

void lcd_write_data(uint8_t data){
    //D/C high for data
    spi_busy_wait(true);
    gpio_put(DISPLAY_DP, 1); //gpio_set(BP_LCD_DP_PORT,BP_LCD_DP_PIN);
    gpio_put(DISPLAY_CS, 0); //gpio_clear(BP_LCD_CS_PORT, BP_LCD_CS_PIN);
    spi_write_blocking(BP_SPI_PORT, &data, 1); //spi_xfer(BP_LCD_SPI, &data);
    gpio_put(DISPLAY_CS, 1); //gpio_set(BP_LCD_CS_PORT, BP_LCD_CS_PIN);
    spi_busy_wait(false);
}

void lcd_disable(void){
    lcd_write_command(0x28);
}

void lcd_enable(void){
    lcd_write_command(0x29);
}

void lcd_screensaver_enable(void){
    lcd_backlight_enable(false);
    lcd_clear();
}

void lcd_screensaver_disable(void){
    if (modes[system_config.mode].protocol_lcd_update){
        modes[system_config.mode].protocol_lcd_update(UI_UPDATE_ALL);
    } else if (displays[system_config.display].display_lcd_update) {
        displays[system_config.display].display_lcd_update(UI_UPDATE_ALL);
    }
    lcd_backlight_enable(true);
}



void lcd_configure(void){
    lcd_write_command(0x36);  //MADCTL (36h): Memory Data Access Control
    lcd_write_data(0b00100000); //0x00 101/011 - left/right hand mode 0b100000

    lcd_write_command(0x3A);//COLMOD,interface pixel format
    lcd_write_data(0x55); //252K

    lcd_write_command(0xB2); //porch setting,, default=0C/0C/00/33/33
    lcd_write_data(0x0c); //was 05
    lcd_write_data(0x0c); //was 05
    lcd_write_data(0x00);
    lcd_write_data(0x33);
    lcd_write_data(0x33);

    lcd_write_command(0xB7); //Gate Control for VGH and VGL setting, default=35
    lcd_write_data(0x70);  //was 0x75, 0x62

    lcd_write_command(0xBB); //VCOMS setting (0.1~1.675 V), default=20
    lcd_write_data(0x21); //was 22

    lcd_write_command(0xC0); //LCM control, default=2C
    lcd_write_data(0x2C);

    lcd_write_command(0xC2); //VDV and VRH command enable, default=01 or FF
    lcd_write_data(0x01);

    lcd_write_command(0xC3); //VRH set (VRH=GVDD), default=0B
    lcd_write_data(0x0B); //was 0x13

    lcd_write_command(0xC4); //VDV set, default=20
    lcd_write_data(0x27); //VDV=0v //was 0x20

    lcd_write_command(0xC6);//FRCTRL=Frame Rate Control in Normal Mode , default=0F
    lcd_write_data(0x0F); //0x11

    lcd_write_command(0xD0);//Power Control, default=A4/81
    lcd_write_data(0xA4);//Constant
    lcd_write_data(0xA1);//AVDD=6.8V;AVCL=-4.8V;VDDS=2.3V

    //lcd_write_command(0xD6);
    //lcd_write_data(0xA1);

    lcd_write_command(0xE0);//PVGAMCTRL:Positive Voltage Gamma Control
    lcd_write_data(0xD0);
    lcd_write_data(0x06); //05
    lcd_write_data(0x0B); //A
    lcd_write_data(0x09);
    lcd_write_data(0x08);
    lcd_write_data(0x30); //05
    lcd_write_data(0x30); //2E
    lcd_write_data(0x5B); //44
    lcd_write_data(0x4B); //45
    lcd_write_data(0x18); //0f
    lcd_write_data(0x14); //17
    lcd_write_data(0x14); //16
    lcd_write_data(0x2C); //2b
    lcd_write_data(0x32); //33

    lcd_write_command(0xE1);//NVGAMCTRL:Negative Voltage Gamma Control
    lcd_write_data(0xD0);
    lcd_write_data(0x05);
    lcd_write_data(0x0A);//0a
    lcd_write_data(0x0A);//09
    lcd_write_data(0x07);//08
    lcd_write_data(0x28);//05
    lcd_write_data(0x32);//2e
    lcd_write_data(0x2C);//43
    lcd_write_data(0x49);//45
    lcd_write_data(0x18);//0f
    lcd_write_data(0x13);//16
    lcd_write_data(0x13);//16
    lcd_write_data(0x2C);//2b
    lcd_write_data(0x33);

    lcd_write_command(0x21);//Display inversion ON ,default=Display inversion OF

    lcd_write_command(0x2A); //Frame rate control
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0xEF);

    lcd_write_command(0x2B); //Display function control
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x01);
    lcd_write_data(0x3F);

    lcd_write_command(0x11);//Sleep out, DC/DC converter, internal oscillator, panel scanning "enable"
    sleep_ms(120);
    lcd_write_command(0x29);//Display ON ,default= Display OFF
}

