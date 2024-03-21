// global config file
#ifndef BP_CONFIG
#define BP_CONFIG

#define BP_FIRMWARE_VERSION "main branch"
#ifndef BP_FIRMWARE_HASH //this variable is for the current commit GIT hash
#define BP_FIRMWARE_HASH _TIMEZ_
#endif

//#define XSTR(x) STR(x)
//#define STR(x) #x
//#pragma message "BP_FIRMWARE_HASH value:" XSTR(BP_FIRMWARE_HASH)

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
#define		BP_USE_HWUSART
#define		BP_USE_HWI2C
//#define		BP_USE_SWI2C
#define		BP_USE_HWSPI
#define		BP_USE_HW2WIRE
//#define		BP_USE_SW2W
//#define		BP_USE_SW3W
//#define 	BP_USE_DIO
#define     BP_USE_HWLED
//#define		BP_USE_LCDSPI
//#define		BP_USE_LCDI2C
//#define		BP_USE_LA
//#define 	BP_USE_DUMMY1
//#define 	BP_USE_DUMMY2
#define     BP_USE_SCOPE

// enable display support
//#define		DISPLAY_USE_HD44780	// is always enabled
//#define		DISPLAY_USE_ST7735

#define OPTARG_STRING_LEN 20

#define BIG_BUFFER_SIZE (128 * 1024)

// include platform
#ifndef BP5_REV
    #error "No /platform/ file included in pirate.h"
#else
    #if BP5_REV == 8
        #include "platform/bpi-rev8.h"
    #elif BP5_REV == 9
        #include "platform/bpi-rev9.h"
    #elif BP5_REV == 10
        #include "platform/bpi-rev10.h"
    #else
        #error "Unknown platform version in pirate.h"
    #endif
#endif

#include "translation/base.h"
#include "printf-4.0.0/printf.h"

void lcd_irq_enable(int16_t repeat_interval);
void lcd_irq_disable(void);
void spi_busy_wait(bool enable);
#endif
