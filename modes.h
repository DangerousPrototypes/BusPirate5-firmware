#include "pirate.h"

enum
{
	HIZ = 0,
#ifdef BP_USE_DUMMY1
	DUMMY1,
#endif
#ifdef BP_USE_DUMMY2
	DUMMY2,
#endif
#ifdef BP_USE_HWSPI
	HWSPI,
#endif
#ifdef BP_USE_HWUSART
	HWUSART,
#endif
#ifdef BP_USE_HWI2C
	HWI2C,
#endif
#ifdef BP_USE_LA
	LA,
#endif
#ifdef BP_USE_SW2W
	SW2W,
#endif
#ifdef BP_USE_SW3W
	SW3W,
#endif
#ifdef BP_USE_DIO
	DIO,
#endif
#ifdef BP_USE_LCDSPI
	LCDSPI,
#endif
#ifdef BP_USE_LCDI2C	// future
	LCDI2C,
#endif
#ifdef BP_USE_SW1WIRE
	SW1WIRE,
#endif
#ifdef BP_USE_HW1WIRE
	HW1WIRE,
#endif
#ifdef BP_USE_HWLED
	HWLED,
#endif
	MAXPROTO
};


typedef struct _mode
{
	void (*protocol_start)(void);			// start
	void (*protocol_startR)(void);			// start with read
	void (*protocol_stop)(void);			// stop
	void (*protocol_stopR)(void);			// stop with read
	uint32_t (*protocol_send)(uint32_t);		// send(/read) max 32 bit
	uint32_t (*protocol_read)(void);		// read max 32 bit
	void (*protocol_clkh)(void);			// set clk high
	void (*protocol_clkl)(void);			// set clk low
	void (*protocol_dath)(void);			// set dat hi
	void (*protocol_datl)(void);			// set dat lo
	uint32_t (*protocol_dats)(void);		// toglle dat (?)
	void (*protocol_clk)(void);			// toggle clk (?)
	uint32_t (*protocol_bitr)(void);		// read 1 bit (?)
	uint32_t (*protocol_periodic)(void);		// service to regular poll whether a byte has arrived or something interesting has happened
	void (*protocol_macro)(uint32_t);		// macro
	uint32_t (*protocol_setup)(void);			// setup UI
	uint32_t (*protocol_setup_exc)(void);		// real setup
	void (*protocol_cleanup)(void);			// cleanup for HiZ
	//const char*(*protocol_pins)(void);			// display pin config
	void (*protocol_settings)(void);		// display settings 
	void (*protocol_help)(void);			// display protocol specific help
	char protocol_name[10];				// friendly name (promptname)
} _mode;

extern struct _mode modes[MAXPROTO];

void nullfunc1(void);
uint32_t nullfunc2(uint32_t c);
uint32_t nullfunc3(void);
void nullfunc4(uint32_t c);
const char *nullfunc5(void);
void nohelp(void);
uint32_t noperiodic(void);
