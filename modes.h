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
    BP_MODE_SW3W,
#endif
#ifdef BP_USE_DIO
    BP_MODE_DIO,
#endif
#ifdef BP_USE_LCDSPI
    BP_MODE_LCDSPI,
#endif
#ifdef BP_USE_LCDI2C	// future
    BP_MODE_LCDI2C,
#endif
#ifdef BP_USE_SW1WIRE
    BP_MODE_SW1WIRE,
#endif
#ifdef BP_USE_HW1WIRE
    BP_MODE_HW1WIRE,
#endif
#ifdef BP_USE_HWLED
    BP_MODE_HWLED,
#endif
#ifdef BP_USE_DUMMY1
    BP_MODE_DUMMY1,
#endif
#ifdef BP_USE_DUMMY2
    BP_MODE_DUMMY2,
#endif
    MAXPROTO
};


typedef struct _mode
{
    void     (*protocol_start     )(bytecode_t *result, bytecode_t *next); // start
    void     (*protocol_start_alt )(bytecode_t *result, bytecode_t *next); // start with read
    void     (*protocol_stop      )(bytecode_t *result, bytecode_t *next); // stop
    void     (*protocol_stop_alt  )(bytecode_t *result, bytecode_t *next); // stop with read
    void     (*protocol_write     )(bytecode_t *result, bytecode_t *next); // send(/read) max 32 bit
    void     (*protocol_read      )(bytecode_t *result, bytecode_t *next); // read max 32 bit
    void     (*protocol_clkh      )(bytecode_t *result, bytecode_t *next); // set clk high
    void     (*protocol_clkl      )(bytecode_t *result, bytecode_t *next); // set clk low
    void     (*protocol_dath      )(bytecode_t *result, bytecode_t *next); // set dat hi
    void     (*protocol_datl      )(bytecode_t *result, bytecode_t *next); // set dat lo
    void     (*protocol_dats      )(bytecode_t *result, bytecode_t *next); // toggle dat (maybe remove?)
    void     (*protocol_tick_clock)(bytecode_t *result, bytecode_t *next); // tick clk
    void     (*protocol_bitr      )(bytecode_t *result, bytecode_t *next); // read dat pin
    
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
} bp_mode_t;

extern const bp_mode_t modes[MAXPROTO];

// Some null functions, for when a mode doesn't need to do anything at a given function
void         nullfunc1  (void      );
uint32_t     nullfunc2  (uint32_t c);
uint32_t     nullfunc3  (void      );
void         nullfunc4  (uint32_t c);
const char * nullfunc5  (void      );
void         nohelp     (void      );
void         noperiodic (void      );
