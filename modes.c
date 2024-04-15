#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "opt_args.h"
#include "commands.h"
#include "modes.h"

#include "mode/hiz.h"
#ifdef	BP_USE_SW1WIRE
    #include "sw1wire.h"
#endif
#ifdef	BP_USE_HW1WIRE
    #include "mode/hw1wire.h"
#endif
#ifdef	BP_USE_HWUART
    #include "mode/hwuart.h"
#endif
#ifdef	BP_USE_HWHDUART
    #include "mode/hwhduart.h"
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
#ifdef	BP_USE_HW2WIRE
    #include "mode/hw2wire.h"
#endif
#ifdef	BP_USE_SW2W
    #include "SW2W.h"
#endif
#ifdef	BP_USE_SW3W
    #include "SW3W.h"
#endif
#ifdef  BP_USE_HWLED
    #include "mode/hwled.h"
#endif
#ifdef BP_USE_DIO
    #include "mode/dio.h"
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
void nullfunc1(void){
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
}

uint32_t nullfunc2(uint32_t c){	
	(void) c;
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
	return 0x0000;
}

uint32_t nullfunc3(void){	
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
	return 0x0000;
}

void nullfunc4(uint32_t c){	
	(void) c;
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
}

const char *nullfunc5(void){
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
}

uint32_t nullfunc6(uint8_t next_command){	
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
	system_config.error=1;
	return 0x0000; 
}

void nohelp(void){
	printf(t[T_MODE_NO_HELP_AVAILABLE]);
}

void noperiodic(void){
	return;
}

void nullfunc1_temp(struct _bytecode *result, struct _bytecode *next){
    printf("%s\r\n", t[T_MODE_ERROR_NO_EFFECT]);
    system_config.error=1;
}

// all modes and their interaction is handled here
// buspirateNG.h has the conditional defines for modes

struct _mode modes[MAXPROTO]={{
	nullfunc1_temp,				// start
	nullfunc1_temp,				// start alternate
	nullfunc1_temp,				// stop
	nullfunc1_temp,				// stop alternate
	nullfunc1_temp,				// write(/read) max 32 bit
	nullfunc1_temp,				// read max 32 bit
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
	noperiodic,				// service to regular poll whether a byte ahs arrived
	nullfunc4,				// macro
	hiz_setup,				// setup UI
	hiz_setup_exec,				// real setup
	hiz_cleanup,				// cleanup for HiZ
	//hiz_pins,				// display pin config
	hiz_settings,				// display settings 
	hiz_help,					// display small help about the protocol
    hiz_commands,                   // mode specific commands
    &hiz_commands_count,                   // mode specific commands count
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
        NULL,                   // mode specific commands
    NULL,                   // mode specific commands count
    "1-WIRE",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HW1WIRE
{
	hw1wire_start,				// start
	hw1wire_start,				// start with read
	nullfunc1_temp,				// stop
	nullfunc1_temp,				// stop with read
	hw1wire_write,				// write(/read) max 32 bit
	hw1wire_read,				// read max 32 bit
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
	noperiodic,				// service to regular poll whether a byte ahs arrived
	hw1wire_macro,				// macro
	hw1wire_setup,				// setup UI
	hw1wire_setup_exc,				// real setup
	hw1wire_cleanup,				// cleanup for HiZ
	//hiz_pins,				// display pin config
	hiz_settings,				// display settings 
	&hw1wire_help,					// display small help about the protocol
    hw1wire_commands,                   // mode specific commands
    &hw1wire_commands_count,                   // mode specific commands count    
    "1-WIRE",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWUART
{
    hwuart_open,				// start
    hwuart_open_read,			// start with read
    hwuart_close,				// stop
    hwuart_close,				// stop with read
    hwuart_write,				// send(/read) max 32 bit
    hwuart_read,				// read max 32 bit
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    hwuart_periodic,				// service to regular poll whether a byte ahs arrived
    hwuart_macro,				// macro
    hwuart_setup,				// setup UI
    hwuart_setup_exc,			// real setup
    hwuart_cleanup,			// cleanup for HiZ
    //hwuart_pins,				// display pin config
    hwuart_settings,			// display settings
    hwuart_help,				// display small help about the protocol
    hwuart_commands,               // mode specific commands
    &hwuart_commands_count,       // mode specific commands count    
    "UART",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWHDUART
{
    hwhduart_open,				// start
    hwhduart_start_alt,			// start with read
    hwhduart_close,				// stop
    hwhduart_stop_alt,				// stop with read
    hwhduart_write,				// send(/read) max 32 bit
    hwhduart_read,				// read max 32 bit
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    hwhduart_periodic,				// service to regular poll whether a byte ahs arrived
    hwhduart_macro,				// macro
    hwhduart_setup,				// setup UI
    hwhduart_setup_exc,			// real setup
    hwhduart_cleanup,			// cleanup for HiZ
    //hwuart_pins,				// display pin config
    hwhduart_settings,			// display settings
    hwhduart_help,				// display small help about the protocol
    hwhduart_commands,               // mode specific commands
    &hwhduart_commands_count,       // mode specific commands count    
    "HDPLXUART",				// friendly name (promptname)
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
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    noperiodic,				// service to regular poll whether a byte ahs arrived
    hwi2c_macro,				// macro
    hwi2c_setup,				// setup UI
    hwi2c_setup_exc,			// real setup
    hwi2c_cleanup,				// cleanup for HiZ
    //HWI2C_pins,				// display pin config
    hwi2c_settings,				// display settings
    hwi2c_help,				// display small help about the protocol
    hwi2c_commands,                   // mode specific commands
    &hwi2c_commands_count,                   // mode specific commands count    
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
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    noperiodic,				// service to regular poll whether a byte ahs arrived
    SWI2C_macro,				// macro
    SWI2C_setup,				// setup UI
    SWI2C_setup_exc,			// real setup
    SWI2C_cleanup,				// cleanup for HiZ
    SWI2C_pins,				// display pin config
    SWI2C_settings,				// display settings
    SWI2C_help,				// display small help about the protocol	
    NULL,                   // mode specific commands
    NULL,                   // mode specific commands count
    "I2C",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HWSPI
{
	spi_start,				// start
	spi_startr,				// start with read
	spi_stop,				// stop
	spi_stopr,				// stop with read
	spi_write,				// send(/read) max 32 bit
	spi_read,				// read max 32 bit
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
	noperiodic,				// service to regular poll whether a byte ahs arrived
	spi_macro,				// macro
	spi_setup,				// setup UI
	spi_setup_exc,			// real setup
	spi_cleanup,				// cleanup for HiZ
	//spi_pins,				// display pin config
	spi_settings,				// display settings 
	spi_help,				// display small help about the protocol
    hwspi_commands,                   // mode specific commands
    &hwspi_commands_count,                   // mode specific commands count
	"SPI",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_HW2WIRE
{
    hw2wire_start,				// start
    hw2wire_start_alt,			// start alternate
    hw2wire_stop,				// stop
    hw2wire_stop_alt,				// stop alternate
    hw2wire_write,				// send(/read) max 32 bit
    hw2wire_read,				// read max 32 bit
    hw2wire_set_clk_high,		// set clk high
    hw2wire_set_clk_low,		// set clk low
    hw2wire_set_dat_high,		// set dat hi
    hw2wire_set_dat_low,		// set dat lo
    nullfunc1_temp,				    // toggle dat (?)
    hw2wire_tick_clock,			// toggle clk (?)
    hw2wire_read_bit,			// read 1 bit (?)
    noperiodic,				    // service to regular poll whether a byte ahs arrived
    hw2wire_macro,				// macro
    hw2wire_setup,				// setup UI
    hw2wire_setup_exc,			// real setup
    hw2wire_cleanup,			// cleanup for HiZ
    //HWI2C_pins,				// display pin config
    hw2wire_settings,			// display settings
    hw2wire_help,				// display small help about the protocol
    hw2wire_commands,            // mode specific commands
    &hw2wire_commands_count,      // mode specific commands count
    "2WIRE",				    // friendly name (promptname)
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
        NULL,                   // mode specific commands
    NULL,                   // mode specific commands count
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
        NULL,                   // mode specific commands
    NULL,                   // mode specific commands count
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
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
	noperiodic,				// service to regular poll whether a byte ahs arrived
	LCDSPI_macro,				// macro
	LCDSPI_setup,				// setup UI
	LCDSPI_setup_exc,			// real setup
	LCDSPI_cleanup,				// cleanup for HiZ
	LCDSPI_pins,				// display pin config
	LCDSPI_settings,			// display settings 
	nohelp,					// display small help about the protocol
        NULL,                   // mode specific commands
    NULL,                   // mode specific commands count
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
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
	noperiodic,				// service to regular poll whether a byte ahs arrived
	LCDI2C_macro,				// macro
	LCDI2C_setup,				// setup UI
	LCDI2C_setup_exc,			// real setup
	LCDI2C_cleanup,				// cleanup for HiZ
	LCDI2C_pins,				// display pin config
	LCDI2C_settings,			// display settings 
	nohelp,					// display small help about the protocol
        NULL,                   // mode specific commands
    NULL,                   // mode specific commands count
	"LCDI2C",				// friendly name (promptname)
},
#endif
#ifdef BP_USE_DIO
{
    nullfunc1_temp,				// start
    nullfunc1_temp,				// start with read
    nullfunc1_temp,				// stop
    nullfunc1_temp,				// stop with read
    dio_write,				// send(/read) max 32 bit
    dio_read,				// read max 32 bit
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    noperiodic,				// service to regular poll whether a byte ahs arrived
    dio_macro,				// macro
    dio_setup,				// setup UI
    dio_setup_exc,				// real setup
    dio_cleanup,				// cleanup for HiZ
    //dio_pins,				// display pin config
    dio_settings,				// display settings
    dio_help,				// display small help about the protocol
    dio_commands,                   // mode specific commands
    &dio_commands_count,                   // mode specific commands count
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
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    noperiodic,				// service to regular poll whether a byte ahs arrived
    hwled_macro,				// macro
    hwled_setup,				// setup UI
    hwled_setup_exc,			// real setup
    hwled_cleanup,				// cleanup for HiZ
    //HWLED_pins,				// display pin config
    hwled_settings,				// display settings
    hwled_help,				// display small help about the protocol
    hwled_commands,                   // mode specific commands
    &hwled_commands_count,                   // mode specific commands count    
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
	nullfunc1_temp,				// set clk high
	nullfunc1_temp,				// set clk low
	nullfunc1_temp,				// set dat hi
	nullfunc1_temp,				// set dat lo
	nullfunc1_temp,				// toggle dat (remove?)
	nullfunc1_temp,				// tick clk
	nullfunc1_temp,				// read dat
    noperiodic,				// service to regular poll whether a byte ahs arrived
    LA_macro,				// macro
    LA_setup,				// setup UI
    LA_setup_exc,				// real setup
    LA_cleanup,				// cleanup for HiZ
    LA_pins,				// display pin config
    LA_settings,				// display settings
    nohelp,					// display small help about the protocol
    NULL,                   // mode specific commands
    NULL,                   // mode specific commands count    
    "LA",					// friendly name (promptname)
},
#endif
#ifdef BP_USE_DUMMY1
{
    dummy1_start,				// start
    dummy1_start,				// start with read
    dummy1_stop,				// stop
    dummy1_stop,				// stop with read
    dummy1_write,				// send(/read) max 32 bit
    dummy1_read,				// read max 32 bit
    dummy1_clkh,				// set clk high
    dummy1_clkl,				// set clk low
    dummy1_dath,				// set dat hi
    dummy1_datl,				// set dat lo
    dummy1_dats,				// toggle dat (?)
    dummy1_clk,				// toggle clk (?)
    dummy1_bitr,				// read 1 bit (?)
    dummy1_periodic,				// service to regular poll whether a byte ahs arrived
    dummy1_macro,				// macro
    dummy1_setup,				// setup UI
    dummy1_setup_exc,			// real setup
    dummy1_cleanup,				// cleanup for HiZ
    //dummy1_pins,				// display pin config
    dummy1_settings,			// display settings
    dummy1_help,				// display small help about the protocol
    dummy1_commands,             // mode specific commands
    &dummy1_commands_count,      // mode specific commands count    
    "DUMMY1",				// friendly name (promptname)
}
#endif

};
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */


