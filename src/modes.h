/**
 * @file modes.h
 * @brief Protocol mode enumeration and dispatch structure
 * 
 * This file defines the protocol mode system including mode IDs
 * and the mode dispatch structure that provides function pointers
 * for all protocol operations.
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

/**
 * @brief Protocol mode enumeration
 * 
 * Defines all available protocol modes. The actual modes included
 * depend on compile-time configuration flags (BP_USE_*).
 * 
 * @note HIZ (high-impedance) is always mode 0
 * @note MAXPROTO defines the total number of modes
 */
enum {
    HIZ = 0,  /**< High-impedance mode (all pins inputs) */
#ifdef BP_USE_HW1WIRE
    HW1WIRE,  /**< 1-Wire protocol (Dallas/Maxim) */
#endif
#ifdef BP_USE_HWUART
    HWUART,   /**< UART protocol */
#endif
#ifdef BP_USE_HWHDUART
    HWHDUART, /**< Half-duplex UART */
#endif
#ifdef BP_USE_HWI2C
    HWI2C,    /**< I2C protocol */
#endif
#ifdef BP_USE_HWSPI
    HWSPI,    /**< SPI protocol */
#endif
#ifdef BP_USE_HW2WIRE
    HW2WIRE,  /**< 2-Wire protocol */
#endif
#ifdef BP_USE_HW3WIRE
    HW3WIRE,  /**< 3-Wire protocol */
#endif
#ifdef BP_USE_DIO
    DIO,      /**< Digital I/O mode */
#endif
#ifdef BP_USE_HWLED
    HWLED,    /**< LED control (WS2812, APA102) */
#endif
#ifdef BP_USE_INFRARED
    INFRARED, /**< Infrared transmit/receive */
#endif
#ifdef BP_USE_JTAG
    JTAG,     /**< JTAG protocol */
#endif
#ifdef BP_USE_I2S
    I2S,      /**< I2S audio */
#endif
#ifdef BP_USE_LCDI2C
    LCDI2C,   /**< LCD I2C (future) */
#endif
#ifdef BP_USE_LCDSPI
    LCDSPI,   /**< LCD SPI */
#endif
#ifdef BP_USE_USBPD
    USBPD,    /**< USB Power Delivery */
#endif
#ifdef BP_USE_DUMMY1
    DUMMY1,   /**< Dummy/test mode */
#endif
    MAXPROTO  /**< Total number of protocol modes */
};

/**
 * @brief Protocol mode dispatch structure
 * 
 * This structure defines all function pointers for a protocol mode,
 * providing a consistent interface for:
 * - Protocol operations (start, stop, read, write)
 * - Pin manipulation (clock, data)
 * - Setup and cleanup
 * - User interface (help, settings)
 * - Mode-specific commands
 */
typedef struct _mode {
    void (*protocol_start)(struct _bytecode* result, struct _bytecode* next);      // start
    void (*protocol_start_alt)(struct _bytecode* result, struct _bytecode* next);  // start with read
    void (*protocol_stop)(struct _bytecode* result, struct _bytecode* next);       // stop
    void (*protocol_stop_alt)(struct _bytecode* result, struct _bytecode* next);   // stop with read
    void (*protocol_write)(struct _bytecode* result, struct _bytecode* next);      // send(/read) max 32 bit
    void (*protocol_read)(struct _bytecode* result, struct _bytecode* next);       // read max 32 bit
    void (*protocol_clkh)(struct _bytecode* result, struct _bytecode* next);       // set clk high
    void (*protocol_clkl)(struct _bytecode* result, struct _bytecode* next);       // set clk low
    void (*protocol_dath)(struct _bytecode* result, struct _bytecode* next);       // set dat hi
    void (*protocol_datl)(struct _bytecode* result, struct _bytecode* next);       // set dat lo
    void (*protocol_dats)(struct _bytecode* result, struct _bytecode* next);       // toggle dat (maybe remove?)
    void (*protocol_tick_clock)(struct _bytecode* result, struct _bytecode* next); // tick clk
    void (*protocol_bitr)(struct _bytecode* result, struct _bytecode* next);       // read dat pin
    void (*protocol_periodic)(void);                                               // service to poll for async data
    void (*protocol_macro)(uint32_t);                                              // macro
    uint32_t (*protocol_setup)(void);                                              // setup UI
    uint32_t (*protocol_setup_exc)(void);                                          // real setup
    void (*protocol_cleanup)(void);                                                // cleanup for HiZ
    // const char*(*protocol_pins)(void);			// display pin config
    void (*protocol_settings)(void);                             // display settings
    void (*protocol_help)(void);                                 // display protocol specific help
    const struct _mode_command_struct(*mode_commands);                // mode specific commands //ignored if 0x00
    const uint32_t(*mode_commands_count);                        // mode specific commands count ignored if 0x00
    void (*protocol_wait_done)(void);                            // wait for the protocol to finish
    uint32_t (*protocol_get_speed)(void);                        // get the current speed setting of the protocol
    char protocol_name[10];                                      // friendly name (promptname)
    uint32_t (*protocol_command)(struct command_result* result); // per mode command parser - ignored if 0
    //void (*protocol_lcd_update)(uint32_t flags);                 // replacement for ui_lcd_update if non-0
    bool (*protocol_preflight_sanity_check)(void); // sanity check before executing syntax
} _mode;

extern struct _mode modes[MAXPROTO];

void nullfunc1(void);
uint32_t nullfunc2(uint32_t c);
uint32_t nullfunc3(void);
void nullfunc4(uint32_t c);
const char* nullfunc5(void);
void nohelp(void);
void noperiodic(void);
