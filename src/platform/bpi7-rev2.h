#ifndef BP_HARDWARE_VERSION

#include <stdint.h>

#define BP_SPLASH_FILE "display/robot7x16.h"

#define BP_HARDWARE_VERSION "Bus Pirate 7"
#define BP_HARDWARE_VERSION_MAJOR 7
#define BP_HARDWARE_VERSION_REVISION 1
#define BP_HARDWARE_MCU "RP2350B"
#define BP_HARDWARE_RAM "512KB"
#define BP_HARDWARE_FLASH "128Mbit"
#define BP_HARDWARE_PSRAM "64Mbit"
#define BP_HARDWARE_PULLUP_VALUE "pull-x"

//font and pin colors
// HEX 24bit RGB format
//used in terminal
#define BP_COLOR_RED 0xdb3030  
#define BP_COLOR_ORANGE 0xdb7530
#define BP_COLOR_YELLOW 0xdbca30
#define BP_COLOR_GREEN 0x30db36
#define BP_COLOR_BLUE 0x305edb
#define BP_COLOR_PURPLE 0x8e30db
#define BP_COLOR_BROWN 0xA0522D
#define BP_COLOR_GREY 0xb3afaf
#define BP_COLOR_WHITE 0xf7f7f7
#define BP_COLOR_BLACK  0x1c1c1c
#define BP_COLOR_FULLBLACK 0x000000
#define BP_COLOR_FULLWHITE 0xffffff

//these have to be ASCII strings for the terminal
//TODO: abuse pre-compiler to use 0x000000 format as above...
#define BP_COLOR_PROMPT_TEXT "150;203;89"
#define BP_COLOR_256_PROMPT_TEXT "113"
#define BP_COLOR_INFO_TEXT "191;165;48"
#define BP_COLOR_256_INFO_TEXT "178"
#define BP_COLOR_NOTICE_TEXT "191;165;48"
#define BP_COLOR_256_NOTICE_TEXT "178"
#define BP_COLOR_WARNING_TEXT "191;165;48"
#define BP_COLOR_256_WARNING_TEXT "178"
#define BP_COLOR_ERROR_TEXT "191;48;48"
#define BP_COLOR_256_ERROR_TEXT "1"
#define BP_COLOR_NUM_FLOAT_TEXT "83;166;230"
#define BP_COLOR_256_NUM_FLOAT_TEXT "26"
#define BP_COLOR_GREY_TEXT "94;94;94"
#define BP_COLOR_256_GREY_TEXT "59"

// LCD size
#define BP_LCD_WIDTH 240
#define BP_LCD_HEIGHT 320

// LCD color pallet 5-6-5 RGB
#define BP_LCD_COLOR_RED    0b1111100000000000
#define BP_LCD_COLOR_ORANGE 0b1111100000000000
#define BP_LCD_COLOR_YELLOW 0b1111100000000000
#define BP_LCD_COLOR_GREEN  0b0000011111100000
#define BP_LCD_COLOR_BLUE   0b0000000000011111
#define BP_LCD_COLOR_PURPLE 0b1111100000011111
#define BP_LCD_COLOR_BROWN  0b1111100000011111
#define BP_LCD_COLOR_GREY   0b1111111111111111
#define BP_LCD_COLOR_WHITE  0b1111111111111111
#define BP_LCD_COLOR_BLACK  0b0000000000000000

#define BP_LCD_REFRESH_RATE_MS -500 //refresh every 500ms, "-"" means reset time at beginning of interrupt

enum bp_pin_func
{
    BP_PIN_IO,
    BP_PIN_MODE,
    BP_PIN_PWM,
    BP_PIN_FREQ,
    BP_PIN_VREF,
    BP_PIN_VOUT,
    BP_PIN_GROUND,
    BP_PIN_DEBUG
};

// Pin names for terminal and LCD display, in order
extern const char hw_pin_label_ordered[10][5];

// name text and background color for the terminal
extern const uint32_t hw_pin_label_ordered_color[10][2];

#define BP_VOUT 0
#define BP_GND 9
#define HW_PINS 10

// Buffer Direction Defines
#define BUFDIR0 0
#define BUFDIR1 1
#define BUFDIR2 2
#define BUFDIR3 3
#define BUFDIR4 4
#define BUFDIR5 5
#define BUFDIR6 6
#define BUFDIR7 7

// Buffer IO defines
#define BUFIO0 8
#define BUFIO1 9
#define BUFIO2 10
#define BUFIO3 11
#define BUFIO4 12
#define BUFIO5 13
#define BUFIO6 14
#define BUFIO7 15   

// these are the buffered io pin short names
enum _bp_bio_pins{
    BIO0,
    BIO1,
    BIO2,
    BIO3,
    BIO4,
    BIO5,
    BIO6,
    BIO7,
    BIO_MAX_PINS
};

// here we map the short names to 
// the buffer IO pin number
extern const uint8_t bio2bufiopin[8];

// here we map the short names to 
// the buffer direction pin number
extern const uint8_t bio2bufdirpin[8];


// SPI Defines
// We are going to use SPI 0 for on-board peripherals
#define BP_SPI_PORT spi0
#define BP_SPI_CDI 32 //16
#define BP_SPI_CLK  34 //18
#define BP_SPI_CDO 35 //19

// NAND flash is on the BP_SPI_PORT, define Chip Select
#define FLASH_STORAGE_CS 31 //26 

// LCD is on the BP_SPI_PORT, define CS and DP pins
#define DISPLAY_CS 29 //25
#define DISPLAY_DP 28 //24
//#define DISPLAY_BACKLIGHT  33
//#define DISPLAY_RESET      36

//pull-up resistor control
//#define PULLUP_EN           42

// Controller data out to SK6812 RGB LEDs
#define RGB_CDO 30 //17
// The number of SK6812 LEDs in the string
#define RGB_LEN 18 

//PWM based PSU control pins
#define PSU_PWM_CURRENT_ADJ 43 //9A
#define PSU_PWM_VREG_ADJ 42 //9B

// moved to I2C IO expander
//#define CURRENT_EN          18
//#define CURRENT_RESET       17
//#define CURRENT_EN_OVERRIDE 16
#define CURRENT_FUSE_DETECT 16
#define BP_I2C_INTERRUPT CURRENT_FUSE_DETECT //REMOVE ME!!!
#define VOUT_ACTIVATE_VOUT 36

//Look behind logic analyzer buffer
#define LA_BPIO0 20
#define LA_BPIO1 21
#define LA_BPIO2 22
#define LA_BPIO3 23
#define LA_BPIO4 24
#define LA_BPIO5 25
#define LA_BPIO6 26
#define LA_BPIO7 27

// A single ADC pin is used to measure the source selected by a 74hct4067
#define AMUX_OUT 45 //28
#define AMUX_OUT_ADC 5

// Current sense ADC
#define CURRENT_SENSE 46 //29
#define CURRENT_SENSE_ADC 6

// Two pins for front buttons
#define EXT1 44 //27

// on bp6 AMUX is controlled directly by MCU pins
#define AMUX_S0             38
#define AMUX_S1             39
#define AMUX_S2             40
#define AMUX_S3             41

//7+ has an internal I2C bus
#define BP_I2C_PORT i2c1
#define BP_I2C_SDA  18
#define BP_I2C_SCL  19
#define BP_I2C_RESET 37
//#define BP_I2C_INTERRUPT 16

//7+ has PSRAM
#define BP_PSRAM_QPI_SS QSPI_SS_2
#define BP_PSRAM_CS 47
//#define RP2350_PSRAM_MAX_SCK_HZ (109 * 1000 * 1000)

// A 74HCT4067 selects one of 16 analog sources to measure 
// (voltage is divided by 2 with a buffered resistor divider)
// ADC connections as they appear on the analog mux pins
// will be disambiguated in the hw_pin_voltages_ordered'
enum adc_mux{
    HW_ADC_MUX_BPIO7, //0
    HW_ADC_MUX_BPIO6, //1
    HW_ADC_MUX_BPIO5, //2
    HW_ADC_MUX_BPIO4, //3
    HW_ADC_MUX_BPIO3, //4
    HW_ADC_MUX_BPIO2, //5
    HW_ADC_MUX_BPIO1, //6
    HW_ADC_MUX_BPIO0, //7
    HW_ADC_MUX_VUSB, //8
    HW_ADC_MUX_VREF_VOUT, //9 //swapped in 7+
    HW_ADC_MUX_VREG_OUT, //10 //swapped in 7+
    //HW_ADC_MUX_CURRENT_DETECT,    //removed in 7+
    HW_ADC_MUX_COUNT
};

#define HW_ADC_MUX_GND 15

// moved to I2C IO expander
// XL9555 IO expander
#define IOEXP_RES_1M 0xff //first 8 bits are 1M resistors
#define IOEXP_DISPLAY_BACKLIGHT 1u << (8+0)
#define IOEXP_DISPLAY_RESET 1u << (8+1)
#define IOEXP_UNUSED_12 1u << (8+2)
#define IOEXP_CURRENT_RESET 1u << (8+3)
#define IOEXP_CURRENT_FUSE_DETECT 1u << (8+4)
#define IOEXP_CURRENT_EN 1u << (8+5)
#define IOEXP_UNUSED_16 1u << (8+6)
#define IOEXP_CURRENT_EN_OVERRIDE 1u << (8+7)

#define bufio2amux(x) (7 - x)

//CURRENT SENSE is attached to a separate ADC, not through the mux
//lets make a define for it (and space in the hw_adc_x arrays) at the end of HW_ADC_MUX_count
#define HW_ADC_CURRENT_SENSE HW_ADC_MUX_COUNT
#define HW_ADC_COUNT  (HW_ADC_MUX_COUNT+1)

//the adc variable holds all the ADC readings
extern uint16_t hw_adc_raw[];
extern uint32_t hw_adc_voltage[];
extern uint32_t hw_adc_avgsum_voltage[];

// this array references the pin voltages in the order that
// they appear in terminal and LCD for easy loop writeout
static const uint32_t* hw_pin_voltage_ordered[]={
    &hw_adc_voltage[HW_ADC_MUX_VREF_VOUT],
    &hw_adc_voltage[HW_ADC_MUX_BPIO0],
    &hw_adc_voltage[HW_ADC_MUX_BPIO1],
    &hw_adc_voltage[HW_ADC_MUX_BPIO2],
    &hw_adc_voltage[HW_ADC_MUX_BPIO3],
    &hw_adc_voltage[HW_ADC_MUX_BPIO4],
    &hw_adc_voltage[HW_ADC_MUX_BPIO5],
    &hw_adc_voltage[HW_ADC_MUX_BPIO6],
    &hw_adc_voltage[HW_ADC_MUX_BPIO7]
};

static const uint32_t* hw_pin_avgsum_voltage_ordered[]={
    &hw_adc_avgsum_voltage[HW_ADC_MUX_VREF_VOUT],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO0],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO1],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO2],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO3],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO4],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO5],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO6],
    &hw_adc_avgsum_voltage[HW_ADC_MUX_BPIO7]
};

//convert raw ADC to volts, for pin with a /2 resistor divider (MUX inputs)
#define hw_adc_to_volts_x2(X) ((6600*hw_adc_raw[X])/4096);
//convert raw ADC to volts, for pin with no resistor divider (Current sense inputs)
#define hw_adc_to_volts_x1(X) ((3300*hw_adc_raw[X])/4096);

//how many 595 shift registers are connected
//#define SHIFT_REG_COUNT 2

#define BP_DEBUG_UART_0 uart0
#define BP_DEBUG_UART_0_TX BIO4
#define BP_DEBUG_UART_0_RX BIO5
#define BP_DEBUG_UART_1 uart1
#define BP_DEBUG_UART_1_TX BIO0 
#define BP_DEBUG_UART_1_RX BIO1     

#define BP_FLASH_DISK_BLOCK_SIZE 2048 //512

void hw_pin_defaults(void);

#endif
