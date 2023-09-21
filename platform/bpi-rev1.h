#ifndef BP_HARDWARE_VERSION

#include "shift.h"

#define BP_HARDWARE_VERSION "Untitled Bus Pirate REV1"
#define BP_HARDWARE_MCU "RP2040"
#define BP_HARDWARE_RAM "264KB"
#define BP_HARDWARE_FLASH "16MB"
#define BP_HARDWARE_PULLUP_VALUE "2Kohms"

//font and pin colors
// HEX 24bit RGB format
//used in terminal and LCD
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
#define BP_COLOR_INFO_TEXT "191;165;48"
#define BP_COLOR_NOTICE_TEXT "191;165;48"
#define BP_COLOR_WARNING_TEXT "191;165;48"
#define BP_COLOR_ERROR_TEXT "191;48;48"
#define BP_COLOR_NUM_FLOAT_TEXT "83;166;230"

// Pin names for terminal and LCD display, in order
//TODO: needs to be const, fix all instances
static const char hw_pin_label_ordered[][5]={
    "Vout",
    "IO0",
    "IO1",
    "IO2",
    "IO3",
    "IO4",
    "IO5",
    "IO6",
    "IO7",
    "GND"
};
static const uint32_t hw_pin_label_ordered_color[][2]={
    {BP_COLOR_FULLBLACK,BP_COLOR_RED},
    {BP_COLOR_FULLBLACK,BP_COLOR_ORANGE},
    {BP_COLOR_FULLBLACK, BP_COLOR_YELLOW},
    {BP_COLOR_FULLBLACK, BP_COLOR_GREEN},
    {BP_COLOR_FULLBLACK,BP_COLOR_BLUE},
    {BP_COLOR_FULLBLACK,BP_COLOR_PURPLE},
    {BP_COLOR_FULLBLACK,BP_COLOR_BROWN},
    {BP_COLOR_FULLBLACK,BP_COLOR_GREY},
    {BP_COLOR_FULLBLACK,BP_COLOR_WHITE},
    {BP_COLOR_FULLWHITE, BP_COLOR_FULLBLACK}
};

#define HW_PINS 10

// Buffer Direction Defines
#define BUFDIR0 0
#define BUFDIR1 1
#define BUFDIR2 6
#define BUFDIR3 7
#define BUFDIR4 8
#define BUFDIR5 9
#define BUFDIR6 14
#define BUFDIR7 15

// Buffer IO defines
#define BUFIO0 2
#define BUFIO1 3
#define BUFIO2 4
#define BUFIO3 5
#define BUFIO4 10
#define BUFIO5 11
#define BUFIO6 12
#define BUFIO7 13   

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
static const uint8_t bio2bufiopin[]=
{
    BUFIO0,
    BUFIO1,
    BUFIO2,
    BUFIO3,
    BUFIO4,
    BUFIO5,
    BUFIO6,
    BUFIO7
};
// here we map the short names to 
// the buffer direction pin number
static const uint8_t bio2bufdirpin[]=
{
    BUFDIR0,
    BUFDIR1,
    BUFDIR2,
    BUFDIR3,
    BUFDIR4,
    BUFDIR5,
    BUFDIR6,
    BUFDIR7
};

// SPI Defines
// We are going to use SPI 0 for on-board peripherals
#define BP_SPI_PORT spi0
#define BP_SPI_CDI 16
#define BP_SPI_CLK  18
#define BP_SPI_CDO 19

// User Flash is on the BP_SPI_PORT, define Chip Select
#define FLASH_CS 24

// LCD is on the BP_SPI_PORT, define CS and DP pins
#define DISPLAY_CS 23
#define DISPLAY_DP 22

// Two 74HC595 shift registers are on BP_SPI_PORT, define latch and enable pins
#define SHIFT_EN 21
#define SHIFT_LATCH 20

// Controller data out to SK6812 RGB LEDs
#define RGB_CDO 17
// The number of SK6812 LEDs in the string
#define RGB_LEN 18 //19 but we're missing one

// Over current detect pin
#define CURRENT_DETECT 25

// A single ADC pin is used to measure the source selected by a 74hct4067
#define AMUX_OUT 28
#define AMUX_OUT_ADC AMUX_OUT - 26

// USB detect pin
#define USB_DET 29

// Two pins brought out to a header
#define EXT0 26
#define EXT1 27

// The two 75hc595 shift registers control various hardware on the board
#define AMUX_EN             0b1
#define AMUX_S0             0b10
#define AMUX_S1             0b100
#define AMUX_S2             0b1000
#define AMUX_S3             0b10000
#define DISPLAY_BACKLIGHT   0b100000
#define DISPLAY_RESET       0b1000000
#define PULLUP_EN           0b10000000
#define DAC_CS              0b100000000
#define CURRENT_EN          0b1000000000
#define CURRENT_TEST        0b10000000000
#define CURRENT_RESET       0b100000000000

// A 74HCT4067 selects one of 16 analog sources to measure (voltage is divided by 2 with a resistor divider)
#define MUX_BPIO0 0
#define MUX_BPIO1 1
#define MUX_BPIO2 2
#define MUX_BPIO3 3
#define MUX_BPIO4 4
#define MUX_BPIO5 5
#define MUX_BPIO6 6
#define MUX_BPIO7 7
#define MUX_VREF_VOUT 8
#define HW_ADC_CURRENT_SENSE 9
#define MUX_VUSB 10
#define MUX_VREG_OUT 11

// ADC connections as they appear on the analog mux pins
// with be disambiguated in the hw_pin_voltages_ordered
enum adc_mux{
    HW_ADC_MUX_BPIO0,
    HW_ADC_MUX_BPIO1,
    HW_ADC_MUX_BPIO2,
    HW_ADC_MUX_BPIO3,
    HW_ADC_MUX_BPIO4,
    HW_ADC_MUX_BPIO5,
    HW_ADC_MUX_BPIO6,
    HW_ADC_MUX_BPIO7,
    HW_ADC_MUX_VREF_VOUT,
    HW_ADC_HW_ADC_CURRENT_SENSE,
    HW_ADC_MUX_VUSB,
    HW_ADC_MUX_VREG_OUT,
    HW_ADC_MUX_MAXADC
};

extern uint16_t hw_adc_raw[HW_ADC_MUX_MAXADC];
extern uint32_t hw_adc_voltage[HW_ADC_MUX_MAXADC];
extern uint32_t* hw_pin_voltage_ordered[];

#define hw_adc_channel_select(x) shift_adc_select(x)

#define SHIFT_REG_COUNT 2

// hardware platform command abstraction
#define HW_BIO_PULLUP_ENABLE() shift_set_clear_wait(PULLUP_EN, 0)    
#define HW_BIO_PULLUP_DISABLE() shift_set_clear_wait(0, PULLUP_EN)
#define delayms(X) busy_wait_ms(X)
#define delayus(X) busy_wait_us_32(X)

void hw_adc_sweep(void);
void hw_jump_to_bootloader(struct command_attributes *attributes, struct command_response *response);

#define BP_DEBUG_ENABLED 1
//#define BP_DEBUG_UART0
#define BP_DEBUG_UART1
#ifdef BP_DEBUG_UART0
    #define BP_DEBUG_UART uart0
    #define BP_DEBUG_UART_TX BIO6
    #define BP_DEBUG_UART_RX BIO7
#endif 
#ifdef BP_DEBUG_UART1
    #define BP_DEBUG_UART uart1
    #define BP_DEBUG_UART_TX BIO2
    #define BP_DEBUG_UART_RX BIO3    
#endif 

#endif