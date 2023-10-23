#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "modes.h"

#include "mode/hiz.h"
#ifdef	BP_USE_SW1WIRE
    #include "sw1wire.h"
#endif
#ifdef	BP_USE_HW1WIRE
    #include "mode/hw1wire.h"
#endif
#ifdef	BP_USE_HWUSART
    #include "mode/usart.h"
#endif
#ifdef	BP_USE_HWI2C
    #include "mode/hwi2c.h"
#endif
#ifdef	BP_USE_SWI2C
	#include "SWI2C.h"
#endif
#ifdef	BP_USE_HWSPI
    #include "mode/hwspi.h"
#endif
#ifdef	BP_USE_SW2W
    #include "SW2W.h"
#endif
#ifdef	BP_USE_SW3W
    #include "SW3W.h"
#endif
#ifdef 	BP_USE_DIO
    #include "DIO.h"
#endif
#ifdef  BP_USE_HWLED
    #include "mode/hwled.h"
#endif
#ifdef	BP_USE_LCDSPI
    #include "LCDSPI.h"
#endif
#ifdef	BP_USE_LCDI2C
    #include "LCDI2C.h"
#endif
#ifdef	BP_USE_LA
    #include "LA.h"
#endif
#ifdef 	BP_USE_DUMMY1
    #include "mode/dummy1.h"
#endif
#ifdef 	BP_USE_DUMMY2
    #include "dummy2.h"
#endif 

// nulfuncs
// these are the dummy functions when something ain't used 
void nullfunc1(void)
{
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
}

uint32_t nullfunc2(uint32_t c)
{	
	(void) c;
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
	return 0x0000;
}

uint32_t nullfunc3(void)
{	
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
	return 0x0000;
}


void nullfunc4(uint32_t c)
{	
	(void) c;
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
}

const char *nullfunc5(void){
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
}

uint32_t nullfunc6(uint8_t next_command)
{	
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
	return 0x0000; 
}

void nohelp(void)
{
	printf(t[T_MODE_NO_HELP_AVAILABLE]);
}

uint32_t noperiodic(void)
{
	return 0;
}

void nullfunc1_temp(struct _bytecode *result, struct _bytecode *next)
{
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
    system_config.error=1;
}

// all modes and their interaction is handled here
// buspirateNG.h has the conditional defines for modes

struct _mode modes[MAXPROTO]={
{
	nullfunc1_temp,				// start
	nullfunc1_temp,				// start with read
	nullfunc1_temp,				// stop
	nullfunc1_temp,				// stop with read
	nullfunc1_temp,				// write(/read) max 32 bit
	nullfunc1_temp,				// read max 32 bit
	nullfunc1,				// set clk high
	nullfunc1,				// set clk low
	nullfunc1,				// set dat hi
	nullfunc1,				// set dat lo
	nullfunc3,				// toggle dat (?)
	nullfunc1,				// toggle clk (?)
	nullfunc3,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	nullfunc4,				// macro
	HiZsetup,				// setup UI
	HiZsetup_exc,				// real setup
	HiZcleanup,				// cleanup for HiZ
	//HiZpins,				// display pin config
	HiZsettings,				// display settings 
	nohelp,					// display small help about the protocol
	"HiZ",					// friendly name (promptname)
},
#ifdef BP_USE_SW1WIRE
{
    ONEWIRE_start,				// start
    ONEWIRE_startr,				// start with read
     ONEWIRE_stop,				// stop
    ONEWIRE_stopr,				// stop with read
    ONEWIRE_send,				// send(/read) max 32 bit
    ONEWIRE_read,				// read max 32 bit
    ONEWIRE_clkh,				// set clk high
    ONEWIRE_clkl,				// set clk low
    ONEWIRE_dath,				// set dat hi
    ONEWIRE_datl,				// set dat lo
    ONEWIRE_dats,				// toggle dat (?)
    ONEWIRE_clk,				// toggle clk (?)
    ONEWIRE_bitr,				// read 1 bit (?)
    ONEWIRE_period,				// service to regular poll whether a byte ahs arrived
    ONEWIRE_macro,				// macro
    ONEWIRE_setup,				// setup UI
    ONEWIRE_setup_exc,			// real setup
    ONEWIRE_cleanup,			// cleanup for HiZ
    ONEWIRE_pins,				// display pin config
    ONEWIRE_settings,			// display settings
    nohelp,					// display small help about the protocol
    "1-WIRE",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HW1WIRE
{
	nullfunc1_temp,				// start
	nullfunc1_temp,				// start with read
	nullfunc1_temp,				// stop
	nullfunc1_temp,				// stop with read
	nullfunc1_temp,				// write(/read) max 32 bit
	nullfunc1_temp,				// read max 32 bit
	nullfunc1,				// set clk high
	nullfunc1,				// set clk low
	nullfunc1,				// set dat hi
	nullfunc1,				// set dat lo
	nullfunc3,				// toggle dat (?)
	nullfunc1,				// toggle clk (?)
	nullfunc3,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	nullfunc4,				// macro
	hw1wire_setup,				// setup UI
	hw1wire_setup_exc,				// real setup
	HiZcleanup,				// cleanup for HiZ
	//HiZpins,				// display pin config
	HiZsettings,				// display settings 
	nohelp,					// display small help about the protocol
    "1-WIRE",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWUSART
{
    hwusart_open,				// start
    hwusart_open_read,				// start with read
    hwusart_close,				// stop
    hwusart_close,				// stop with read
    hwusart_write,				// send(/read) max 32 bit
    hwusart_read,				// read max 32 bit
    nullfunc1,				// set clk high
    nullfunc1,				// set clk low
    nullfunc1,				// set dat hi
    nullfunc1,				// set dat lo
    nullfunc3,				// toggle dat (?)
    nullfunc1,				// toggle clk (?)
    nullfunc3,				// read 1 bit (?)
    hwusart_periodic,				// service to regular poll whether a byte ahs arrived
    hwusart_macro,				// macro
    hwusart_setup,				// setup UI
    hwusart_setup_exc,			// real setup
    hwusart_cleanup,			// cleanup for HiZ
    //HWUSART_pins,				// display pin config
    hwusart_settings,			// display settings
    hwusart_help,				// display small help about the protocol
    "UART",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWI2C
{
    hwi2c_start,				// start
    hwi2c_start,				// start with read
    hwi2c_stop,				// stop
    hwi2c_stop,				// stop with read
    hwi2c_write,				// send(/read) max 32 bit
    hwi2c_read,				// read max 32 bit
    nullfunc1,				// set clk high
    nullfunc1,				// set clk low
    nullfunc1,				// set dat hi
    nullfunc1,				// set dat lo
    nullfunc3,				// toggle dat (?)
    nullfunc1,				// toggle clk (?)
    nullfunc3,				// read 1 bit (?)
    noperiodic,				// service to regular poll whether a byte ahs arrived
    hwi2c_macro,				// macro
    hwi2c_setup,				// setup UI
    hwi2c_setup_exc,			// real setup
    hwi2c_cleanup,				// cleanup for HiZ
    //HWI2C_pins,				// display pin config
    hwi2c_settings,				// display settings
    hwi2c_help,				// display small help about the protocol
    "I2C",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_SWI2C
{
    SWI2C_start,				// start
    SWI2C_start,				// start with read
     SWI2C_stop,				// stop
    SWI2C_stop,				// stop with read
    SWI2C_write,				// swrite(/read) max 32 bit
    SWI2C_read,				// read max 32 bit
    nullfunc1,				// set clk high
    nullfunc1,				// set clk low
    nullfunc1,				// set dat hi
    nullfunc1,				// set dat lo
    nullfunc3,				// toggle dat (?)
    nullfunc1,				// toggle clk (?)
    nullfunc3,				// read 1 bit (?)
    noperiodic,				// service to regular poll whether a byte ahs arrived
    SWI2C_macro,				// macro
    SWI2C_setup,				// setup UI
    SWI2C_setup_exc,			// real setup
    SWI2C_cleanup,				// cleanup for HiZ
    SWI2C_pins,				// display pin config
    SWI2C_settings,				// display settings
    SWI2C_help,				// display small help about the protocol	
    "I2C",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWSPI
{
	spi_start,				// start
	spi_start,				// start with read
	spi_stop,				// stop
	spi_stop,				// stop with read
	spi_write,				// send(/read) max 32 bit
	spi_read,				// read max 32 bit
	nullfunc1,				// set clk high
	nullfunc1,				// set clk low
	nullfunc1,				// set dat hi
	nullfunc1,				// set dat lo
	nullfunc3,				// toggle dat (?)
	nullfunc1,				// toggle clk (?)
	nullfunc3,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	spi_macro,				// macro
	spi_setup,				// setup UI
	spi_setup_exc,			// real setup
	spi_cleanup,				// cleanup for HiZ
	//spi_pins,				// display pin config
	spi_settings,				// display settings 
	spi_help,				// display small help about the protocol
	"SPI",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_SW2W
{
	SW2W_start,				// start
	SW2W_startr,				// start with read
 	SW2W_stop,				// stop
	SW2W_stopr,				// stop with read
	SW2W_send,				// send(/read) max 32 bit
	SW2W_read,				// read max 32 bit
	SW2W_clkh,				// set clk high
	SW2W_clkl,				// set clk low
	SW2W_dath,				// set dat hi
	SW2W_datl,				// set dat lo
	SW2W_dats,				// toggle dat (?)
	SW2W_clk,				// toggle clk (?)
	SW2W_bitr,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	SW2W_macro,				// macro
	SW2W_setup,				// setup UI
	SW2W_setup_exc,				// real setup
	SW2W_cleanup,				// cleanup for HiZ
	SW2W_pins,				// display pin config
	SW2W_settings,				// display settings 
	nohelp,					// display small help about the protocol
	"2WIRE",					// friendly name (promptname)
},
#endif
#ifdef BP_USE_SW3W
{
	SW3W_start,				// start
	SW3W_startr,				// start with read
 	SW3W_stop,				// stop
	SW3W_stopr,				// stop with read
	SW3W_send,				// send(/read) max 32 bit
	SW3W_read,				// read max 32 bit
	SW3W_clkh,				// set clk high
	SW3W_clkl,				// set clk low
	SW3W_dath,				// set dat hi
	SW3W_datl,				// set dat lo
	SW3W_dats,				// toggle dat (?)
	SW3W_clk,				// toggle clk (?)
	SW3W_bitr,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	SW3W_macro,				// macro
	SW3W_setup,				// setup UI
	SW3W_setup_exc,				// real setup
	SW3W_cleanup,				// cleanup for HiZ
	SW3W_pins,				// display pin config
	SW3W_settings,				// display settings 
	SW3W_help,				// display small help about the protocol
	"3WIRE",					// friendly name (promptname)
},
#endif
#ifdef BP_USE_LCDSPI
{
	nullfunc1,				// start
	nullfunc1,				// start with read
 	nullfunc1,				// stop
	nullfunc1,				// stop with read
	LCDSPI_send,				// send(/read) max 32 bit
	LCDSPI_read,				// read max 32 bit
	nullfunc1,				// set clk high
	nullfunc1,				// set clk low
	nullfunc1,				// set dat hi
	nullfunc1,				// set dat lo
	nullfunc3,				// toggle dat (?)
	nullfunc1,				// toggle clk (?)
	nullfunc3,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	LCDSPI_macro,				// macro
	LCDSPI_setup,				// setup UI
	LCDSPI_setup_exc,			// real setup
	LCDSPI_cleanup,				// cleanup for HiZ
	LCDSPI_pins,				// display pin config
	LCDSPI_settings,			// display settings 
	nohelp,					// display small help about the protocol
	"LCDSPI",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_LCDI2C
{
	nullfunc1,				// start
	nullfunc1,				// start with read
 	nullfunc1,				// stop
	nullfunc1,				// stop with read
	LCDI2C_send,				// send(/read) max 32 bit
	LCDI2C_read,				// read max 32 bit
	nullfunc1,				// set clk high
	nullfunc1,				// set clk low
	nullfunc1,				// set dat hi
	nullfunc1,				// set dat lo
	nullfunc3,				// toggle dat (?)
	nullfunc1,				// toggle clk (?)
	nullfunc3,				// read 1 bit (?)
	noperiodic,				// service to regular poll whether a byte ahs arrived
	LCDI2C_macro,				// macro
	LCDI2C_setup,				// setup UI
	LCDI2C_setup_exc,			// real setup
	LCDI2C_cleanup,				// cleanup for HiZ
	LCDI2C_pins,				// display pin config
	LCDI2C_settings,			// display settings 
	nohelp,					// display small help about the protocol
	"LCDI2C",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_DIO
{
    nullfunc1,				// start
    nullfunc1,				// start with read
     nullfunc1,				// stop
    nullfunc1,				// stop with read
    DIO_send,				// send(/read) max 32 bit
    DIO_read,				// read max 32 bit
    nullfunc1,				// set clk high
    nullfunc1,				// set clk low
    nullfunc1,				// set dat hi
    nullfunc1,				// set dat lo
    nullfunc3,				// toggle dat (?)
    nullfunc1,				// toggle clk (?)
    nullfunc3,				// read 1 bit (?)
    noperiodic,				// service to regular poll whether a byte ahs arrived
    DIO_macro,				// macro
    DIO_setup,				// setup UI
    DIO_setup_exc,				// real setup
    DIO_cleanup,				// cleanup for HiZ
    DIO_pins,				// display pin config
    DIO_settings,				// display settings
    DIO_help,				// display small help about the protocol
    "DIO",					// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWLED
{
    hwled_start,				// start
    hwled_start,				// start with read
    hwled_stop,				// stop
    hwled_stop,				// stop with read
    hwled_write,				// send(/read) max 32 bit
    nullfunc1_temp,				// read max 32 bit
    nullfunc1,				// set clk high
    nullfunc1,				// set clk low
    nullfunc1,				// set dat hi
    nullfunc1,				// set dat lo
    nullfunc3,				// toggle dat (?)
    nullfunc1,				// toggle clk (?)
    nullfunc3,				// read 1 bit (?)
    noperiodic,				// service to regular poll whether a byte ahs arrived
    hwled_macro,				// macro
    hwled_setup,				// setup UI
    hwled_setup_exc,			// real setup
    hwled_cleanup,				// cleanup for HiZ
    //HWLED_pins,				// display pin config
    hwled_settings,				// display settings
    hwled_help,				// display small help about the protocol
    "LED",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_LA
{
    nullfunc1,				// start
    nullfunc1,				// start with read
     nullfunc1,				// stop
    nullfunc1,				// stop with read
    nullfunc2,				// send(/read) max 32 bit
    nullfunc3,				// read max 32 bit
    nullfunc1,				// set clk high
    nullfunc1,				// set clk low
    nullfunc1,				// set dat hi
    nullfunc1,				// set dat lo
    nullfunc3,				// toggle dat (?)
    nullfunc1,				// toggle clk (?)
    nullfunc3,				// read 1 bit (?)
    noperiodic,				// service to regular poll whether a byte ahs arrived
    LA_macro,				// macro
    LA_setup,				// setup UI
    LA_setup_exc,				// real setup
    LA_cleanup,				// cleanup for HiZ
    LA_pins,				// display pin config
    LA_settings,				// display settings
    nohelp,					// display small help about the protocol
    "LA",					// friendly name (promptname)
},
#endif
#ifdef BP_USE_DUMMY1
{
    dummy1_start,				// start
    dummy1_startr,				// start with read
     dummy1_stop,				// stop
    dummy1_stopr,				// stop with read
    dummy1_send,				// send(/read) max 32 bit
    dummy1_read,				// read max 32 bit
    dummy1_clkh,				// set clk high
    dummy1_clkl,				// set clk low
    dummy1_dath,				// set dat hi
    dummy1_datl,				// set dat lo
    dummy1_dats,				// toggle dat (?)
    dummy1_clk,				// toggle clk (?)
    dummy1_bitr,				// read 1 bit (?)
    dummy1_period,				// service to regular poll whether a byte ahs arrived
    dummy1_macro,				// macro
    dummy1_setup,				// setup UI
    dummy1_setup_exc,			// real setup
    dummy1_cleanup,				// cleanup for HiZ
    //dummy1_pins,				// display pin config
    dummy1_settings,			// display settings
    nohelp,					// display small help about the protocol
    "DUMMY1",				// friendly name (promptname)
}
#endif

};





