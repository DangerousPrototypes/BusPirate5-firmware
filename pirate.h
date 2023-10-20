// global config file
#ifndef BP_CONFIG
#define BP_CONFIG

#define BP_FIRMWARE_VERSION "v0.1.0"
#ifndef BP_FIRMWARE_HASH //this variable is for the current revision GIT hash
#define BP_FIRMWARE_HASH "commit unknown"
#endif

// UI stuff
#define UI_CMDBUFFSIZE	512		// must be power of 2

// USB VID/PID
#define		USB_VID		0x1209
#define		USB_PID		0x7331
#define		USB_VENDOR	"BusPirate.com"
#define		USB_PRODUCT	"BusPirate5"

// enable modes
//#define		BP_USE_SW1WIRE
//#define     BP_USE_HW1WIRE
#define		BP_USE_HWUSART
#define		BP_USE_HWI2C
//#define		BP_USE_SWI2C
#define		BP_USE_HWSPI
//#define		BP_USE_SW2W
//#define		BP_USE_SW3W
//#define 	BP_USE_DIO
#define     BP_USE_HWLED
//#define		BP_USE_LCDSPI
//#define		BP_USE_LCDI2C
//#define		BP_USE_LA
//#define 	BP_USE_DUMMY1
//#define 	BP_USE_DUMMY2

// enable display support
//#define		DISPLAY_USE_HD44780	// is always enabled
//#define		DISPLAY_USE_ST7735

#define OPTARG_STRING_LEN 20

// include platform
#include "platform/bpi-rev8.h"

// include a translation
//new multilingual system
#include "translation/base.h"

#include "printf-4.0.0/printf.h"

void lcd_irq_enable(int16_t repeat_interval);
void lcd_irq_disable(void);

void spi_busy_wait(bool enable);

#endif