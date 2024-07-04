//#include "pirate.h"

enum
{
    BP_MODE_HIZ = 0,
#ifdef BP_USE_HWSPI
    BP_MODE_HWSPI,
#endif
#ifdef BP_USE_HWUART
    BP_MODE_HWUART,
#endif
#ifdef BP_USE_HWHDUART
    BP_MODE_HWHDUART,
#endif
#ifdef BP_USE_HWI2C
    BP_MODE_HWI2C,
#endif
#ifdef BP_USE_LA
    BP_MODE_LA,
#endif
#ifdef BP_USE_HW2WIRE
    BP_MODE_HW2WIRE,
#endif
#ifdef BP_USE_SW2W
    BP_MODE_SW2W,
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
#ifdef BP_USE_DUMMY1
    DUMMY1,
#endif
#ifdef BP_USE_DUMMY2
    DUMMY2,
#endif
    MAXPROTO
};


typedef struct _mode
{
    void     (*protocol_start     )(struct _bytecode *result, struct _bytecode *next); // start
    void     (*protocol_start_alt )(struct _bytecode *result, struct _bytecode *next); // start with read
    void     (*protocol_stop      )(struct _bytecode *result, struct _bytecode *next); // stop
    void     (*protocol_stop_alt  )(struct _bytecode *result, struct _bytecode *next); // stop with read
    void     (*protocol_write     )(struct _bytecode *result, struct _bytecode *next); // send(/read) max 32 bit
    void     (*protocol_read      )(struct _bytecode *result, struct _bytecode *next); // read max 32 bit
    void     (*protocol_clkh      )(struct _bytecode *result, struct _bytecode *next); // set clk high
    void     (*protocol_clkl      )(struct _bytecode *result, struct _bytecode *next); // set clk low
    void     (*protocol_dath      )(struct _bytecode *result, struct _bytecode *next); // set dat hi
    void     (*protocol_datl      )(struct _bytecode *result, struct _bytecode *next); // set dat lo
    void     (*protocol_dats      )(struct _bytecode *result, struct _bytecode *next); // toggle dat (maybe remove?)
    void     (*protocol_tick_clock)(struct _bytecode *result, struct _bytecode *next); // tick clk
    void     (*protocol_bitr      )(struct _bytecode *result, struct _bytecode *next); // read dat pin
    
    void     (*protocol_periodic  )(void    );     // service to regular poll whether a byte has arrived or something interesting has happened
    void     (*protocol_macro     )(uint32_t);     // macro
    uint32_t (*protocol_setup     )(void    );     // setup UI
    uint32_t (*protocol_setup_exc )(void    );     // real setup
    void     (*protocol_cleanup   )(void    );     // cleanup for HiZ
    //const char*(*protocol_pins    )(void    );     // display pin config
    void     (*protocol_settings  )(void    );     // display settings 
    void     (*protocol_help      )(void    );     // display protocol specific help
    const struct _command_struct (*mode_commands); // mode specific commands //ignored if 0x00
    const uint32_t (*mode_commands_count);         // mode specific commands count ignored if 0x00
    char protocol_name[10];                        // friendly name (promptname)

    uint32_t (*protocol_command   )(struct command_result *result); // per mode command parser - ignored if nullptr
    void     (*protocol_lcd_update)(uint32_t flags);	// replacement for ui_lcd_update if non-nullptr
} _mode;

extern struct _mode modes[MAXPROTO];

// Some null functions, for when a mode doesn't need to do anything at a given function
void         nullfunc1  (void      );
uint32_t     nullfunc2  (uint32_t c);
uint32_t     nullfunc3  (void      );
void         nullfunc4  (uint32_t c);
const char * nullfunc5  (void      );
void         nohelp     (void      );
void         noperiodic (void      );
