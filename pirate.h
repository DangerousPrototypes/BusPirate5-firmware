// global config file
#ifndef BP_CONFIG
#define BP_CONFIG

#define BP_FIRMWARE_VERSION "main branch"

#ifndef BP_FIRMWARE_HASH //this variable is for the current commit GIT hash
#define BP_FIRMWARE_HASH "unknown"
#endif
#ifndef BP_FIRMWARE_TIMESTAMP //this variable is for the compile timestamp
#define BP_FIRMWARE_TIMESTAMP _TIMEZ_
#endif

#define BP_SPI_START_SPEED 1000 * 1000
#define BP_SPI_HIGH_SPEED 1000*1000*16
#define BP_SPI_SHIFT_SPEED 1000*1000*16
//#define XSTR(x) STR(x)
//#define STR(x) #x
//#pragma message "BP_FIRMWARE_HASH value:" XSTR(BP_FIRMWARE_HASH)
#define BP_FILENAME_MAX 13

// UI stuff
#define UI_CMDBUFFSIZE	512		// must be power of 2

// USB VID/PID
#define		USB_VID		0x1209
#define		USB_PID		0x7331
#define		USB_VENDOR	"BusPirate.com"
#define		USB_PRODUCT	"BusPirate5"

// enable modes
//#define		BP_USE_SW1WIRE
#define     BP_USE_HW1WIRE
#define		BP_USE_HWUART
#define     BP_USE_HWHDUART
#define		BP_USE_HWI2C
//#define		BP_USE_SWI2C
#define		BP_USE_HWSPI
#define		BP_USE_HW2WIRE
//#define		BP_USE_SW2W
//#define		BP_USE_SW3W
#define     BP_USE_HWLED
//#define		BP_USE_LCDSPI
//#define		BP_USE_LCDI2C
//#define		BP_USE_LA
#define     BP_USE_DIO
#define     BP_USE_INFRARED
//#define 	BP_USE_DUMMY1
//#define 	BP_USE_DUMMY2
#define     BP_USE_SCOPE
//#define     BP_USE_BINLOOPBACK

// enable display support
//#define		DISPLAY_USE_HD44780	// is always enabled
//#define		DISPLAY_USE_ST7735

#define BIG_BUFFER_SIZE (128 * 1024)

// include platform
#ifndef BP_BOARD_REVISION
    #error "No /platform/ file included in pirate.h"
#else
    #if BP_VERSION == BP5
        #if BP_BOARD_REVISION == 8
            #include "platform/bpi-rev8.h"
        #elif BP_BOARD_REVISION == 9
            #include "platform/bpi-rev9.h"
        #elif BP_BOARD_REVISION == 10
            #include "platform/bpi-rev10.h"
        #else
            #error "Unknown platform version in pirate.h"
        #endif
    #elif BP_VERSION == BP5XL
        #include "platform/bpi-rev10.h"
    #elif BP_VERSION == BP6
        #include "platform/bpi6-rev1.h"
    #else
        #error "Unknown platform version in pirate.h"
    #endif
#endif

#include "translation/base.h"
#include "printf-4.0.0/printf.h"
#include <stdint.h>
#include <stdbool.h>

void lcd_irq_enable(int16_t repeat_interval);
void lcd_irq_disable(void);
void spi_busy_wait(bool enable);

// 1wire settings
#define M_OW_PIO pio0
#define M_OW_PIO_SM 3
#define M_OW_OWD BIO3

// UART settings
#define M_UART_PORT uart0
#define M_UART_TX BIO4
#define M_UART_RX BIO5
#define M_UART_RTS
#define M_UART_CTS
#define M_UART_PIO pio0
#define M_UART_PIO_SM 3
#define M_UART_RXTX BIO0

// i2c settings
#define M_I2C_PIO pio0
#define M_I2C_PIO_SM 3
#define M_I2C_SDA BIO0
#define M_I2C_SCL BIO1

// SPI settings
#define M_SPI_PORT spi1
#define M_SPI_CLK BIO6
#define M_SPI_CDO BIO7
#define M_SPI_CDI BIO4
#define M_SPI_CS BIO5
#define M_SPI_SELECT 0
#define M_SPI_DESELECT 1

// 2wire settings
#define M_2WIRE_PIO pio0
#define M_2WIRE_PIO_SM 3  
#define M_2WIRE_SDA BIO0
#define M_2WIRE_SCL BIO1
#define M_2WIRE_RST BIO2

// LED settings
#define M_LED_PIO pio0
#define M_LED_PIO_SM 3
#define M_LED_SDO BIO0
#define M_LED_SCL BIO1 //only used on APA102


#endif
