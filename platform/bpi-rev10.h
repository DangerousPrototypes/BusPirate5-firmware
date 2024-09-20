#ifndef BP_HARDWARE_VERSION

#define BP_HARDWARE_VERSION "Bus Pirate 5 REV10"
#define BP_HARDWARE_MCU "RP2040"
#define BP_HARDWARE_RAM "264KB"
#define BP_HARDWARE_FLASH "128Mbit"
#define BP_HARDWARE_PULLUP_VALUE "10K ohms"

//font and pin colors
// HEX 24bit RGB format
//used in terminal
//BUGBUG -- prefix should be BP_COLOR_RGB24_ to distinguish from ANSI_RGB_24_, ANSI_256_, LCD_RGB565_, etc.
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

//BUGBUG ... the ANSI_RGB_24 attribute sequence should be of the form: ";2;r;g;b", where:
//           ';2' indicates the FORMAT of the color as 24-bit color with three control segments
//           ';r' is the red   control segment, with `r` being a decimal value in range [0..255]
//           ';g' is the green control segment, with `g` being a decimal value in range [0..255]
//           ';b' is the blue  control segment, with `b` being a decimal value in range [0..255]
//           Requires updating other macros to reflect the inclusion of ";2"
//BUGBUG ... the ANSI_256 attribute sequence should be of the form: ";5;n", where:
//           ';5' indicates the FORMAT of the color as 256-color with one control segment
//           ';n' is the color index, with `n` being a decimal value in range [0..255]
//           Requires updating other macros to reflect the inclusion of ";5"

//BUGBUG -- prefix should be ANSI_RGB_24_ or ANSI_256_ to distinguish from BP_COLOR_RGB24_, LCD_RGB565_, etc.
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

// LCD color pallet 5-6-5 RGB
//BUGBUG -- prefix should be LCD_RGB565_ to standardize with BP_COLOR_RGB24_, ANSI_RGB_24_, ANSI_256_, etc.
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
// BUGBUG -- rename these to BP_PIN_SPI_*
#define BP_SPI_PORT spi0
#define BP_SPI_CDI 16
#define BP_SPI_CLK  18
#define BP_SPI_CDO 19

// NAND flash is on the BP_SPI_PORT, define Chip Select
// BUGBUG -- rename to BP_PIN_FLASH_STORAGE_CS
#define FLASH_STORAGE_CS 26 

// LCD is on the BP_SPI_PORT, define CS and DP pins
// BUGBUG -- rename these to BP_PIN_DISPLAY_*
#define DISPLAY_CS 25
#define DISPLAY_DP 24

// Two 74HC595 shift registers are on BP_SPI_PORT, define latch and enable pins
// BUGBUG -- rename these to BP_PIN_SHIFT_*
#define SHIFT_EN 21
#define SHIFT_LATCH 20

// Controller data out to SK6812 RGB LEDs
// BUGBUG -- rename to BP_PIN_PIXEL_DATA
#define RGB_CDO 17
// The number of SK6812 LEDs in the string
// BUGBUG -- rename to BP_PIXEL_COUNT
#define RGB_LEN 18 

//PWM based PSU control pins
// BUGBUG -- rename these to BP_PIN_PSU_PWM_*
#define PSU_PWM_CURRENT_ADJ 22 //3A
#define PSU_PWM_VREG_ADJ 23 //3B

//First pin (base) of logic analyzer input
// BUGBUG -- rename to BP_PIN_LA_BPIO0
#define LA_BPIO0 8

// A single ADC pin is used to measure the source selected by a 74hct4067
// BUGBUG -- rename these to BP_PIN_AMUX_*
#define AMUX_OUT 28
#define AMUX_OUT_ADC (AMUX_OUT - 26)

// Current sense ADC
// BUGBUG -- rename these to BP_PIN_CURRENT_SENSE_*
#define CURRENT_SENSE 29
#define CURRENT_SENSE_ADC (CURRENT_SENSE - 26)

// One pin for front button
// BUGBUG -- rename to BP_PIN_FRONT_BUTTON
#define EXT1 27

// The two 75hc595 shift registers control various hardware on the board
// BUGBUG -- rename these to have a comon prefix, such as BP_SHIFT_*
#define AMUX_EN             (1u<<0)
#define AMUX_S0             (1u<<1)
#define AMUX_S1             (1u<<2)
#define AMUX_S2             (1u<<3)
#define AMUX_S3             (1u<<4)
#define DISPLAY_BACKLIGHT   (1u<<5)
#define DISPLAY_RESET       (1u<<6)
#define PULLUP_EN           (1u<<7)
//#define                   (1u<<8) 
#define CURRENT_EN          (1u<<9)
//#define                   (1u<<10)
#define CURRENT_RESET       (1u<<11)
//#define DAC_CS              (1u<<12)
#define CURRENT_EN_OVERRIDE (1u<<13)
//#define                   (1u<<14)
//#define                   (1u<<15)

// A 74HCT4067 selects one of 16 analog sources to measure 
// (voltage is divided by 2 with a buffered resistor divider)
// ADC connections as they appear on the analog mux pins
// will be disambiguated in the hw_pin_voltages_ordered'
enum adc_mux{
    HW_ADC_MUX_BPIO7,          //  0
    HW_ADC_MUX_BPIO6,          //  1
    HW_ADC_MUX_BPIO5,          //  2
    HW_ADC_MUX_BPIO4,          //  3
    HW_ADC_MUX_BPIO3,          //  4
    HW_ADC_MUX_BPIO2,          //  5
    HW_ADC_MUX_BPIO1,          //  6
    HW_ADC_MUX_BPIO0,          //  7
    HW_ADC_MUX_VUSB,           //  8
    HW_ADC_MUX_CURRENT_DETECT, //  9
    HW_ADC_MUX_VREG_OUT,       // 10
    HW_ADC_MUX_VREF_VOUT,      // 11
    HW_ADC_MUX_COUNT           // Total: 12 values
};
#define HW_ADC_MUX_GND 15
static_assert(HW_ADC_MUX_COUNT < HW_ADC_MUX_GND, "HW_ADC_MUX_COUNT must be 12");

// BUGBUG -- make this an inline function that returns the enum type for (at least) better self-documentation
#define bufio2amux(x) (7 - x) // converts buffer IO [0..8] number to enum value for HW_ADC_MUX_BPIOx

//CURRENT SENSE is attached to a separate ADC (not through the mux)
//lets make a define for it (and space in the HW_ADC_hw_adc_x arrays) at the end of HW_ADC_MUX_count
#define HW_ADC_CURRENT_SENSE  HW_ADC_MUX_COUNT
#define HW_ADC_COUNT         (HW_ADC_MUX_COUNT+1) // size of the below buffers

//the adc variable holds all the ADC readings
extern uint16_t hw_adc_raw[];               // BUGBUG ... can we define size of the array here?  (HW_ADC_COUNT)
extern uint32_t hw_adc_voltage[];           // BUGBUG ... can we define size of the array here?  (HW_ADC_COUNT)

// this array references the pin voltages in the order that
// they appear in terminal and LCD for easy loop writeout
extern uint32_t *hw_pin_voltage_ordered[]; // BUGBUG: what's the safe way to index this array?  Wish could use C++ and strongly-typed indices to prevent errors.

//convert raw ADC to volts, for pin with a /2 resistor divider (MUX inputs)
// BUGBUG -- make this an inline function for (at least) better self-documentation, type safety, debugging, etc.
// BUGBUG -- Valid indices for `X` is [0..HW_ADC_MUX_COUNT-1].  Document and assert this.
#define hw_adc_to_volts_x2(X) ((6600*hw_adc_raw[X])/4096);
//convert raw ADC to volts, for pin with no resistor divider (Current sense inputs)
// BUGBUG -- make this an inline function for (at least) better self-documentation, type safety, debugging, etc.
// BUGBUG -- ONLY valid value for `X` is HW_ADC_CURRENT_SENSE.  Document and assert this.
#define hw_adc_to_volts_x1(X) ((3300*hw_adc_raw[X])/4096);

//how many 595 shift registers are connected
#define SHIFT_REG_COUNT 2 // BUGBUG -- remove unused defines?

#define BP_DEBUG_UART_0    uart0
#define BP_DEBUG_UART_0_TX BIO4
#define BP_DEBUG_UART_0_RX BIO5
#define BP_DEBUG_UART_1    uart1
#define BP_DEBUG_UART_1_TX BIO0 
#define BP_DEBUG_UART_1_RX BIO1     

#define BP_FLASH_DISK_BLOCK_SIZE 2048

#endif
