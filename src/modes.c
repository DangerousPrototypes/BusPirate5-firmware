#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "bytecode.h"
#include "command_struct.h"
#include "commands.h"
#include "modes.h"

#include "mode/hiz.h"
#ifdef BP_USE_HW1WIRE
#include "mode/hw1wire.h"
#endif
#ifdef BP_USE_HWUART
#include "mode/hwuart.h"
#endif
#ifdef BP_USE_HWHDUART
#include "mode/hwhduart.h"
#endif
#ifdef BP_USE_HWI2C
#include "mode/hwi2c.h"
#endif
#ifdef BP_USE_HWSPI
#include "mode/hwspi.h"
#endif
#ifdef BP_USE_HW2WIRE
#include "mode/hw2wire.h"
#endif
#ifdef BP_USE_HW3WIRE
#include "mode/hw3wire.h"
#endif
#ifdef BP_USE_HWLED
#include "mode/hwled.h"
#endif
#ifdef BP_USE_DIO
#include "mode/dio.h"
#endif
#ifdef BP_USE_INFRARED
#include "mode/infrared.h"
#endif
#ifdef BP_USE_LCDSPI
#include "LCDSPI.h"
#endif
#ifdef BP_USE_LCDI2C
#include "LCDI2C.h"
#endif
#ifdef BP_USE_DUMMY1
#include "mode/dummy1.h"
#endif
#ifdef BP_USE_BINLOOPBACK
#include "mode/binloopback.h"
#endif
#ifdef BP_USE_JTAG
#include "mode/jtag.h"
#endif

// nulfuncs
// these are the dummy functions when something ain't used
void nullfunc1(void) {
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
}

uint32_t nullfunc2(uint32_t c) {
    (void)c;
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
    return 0x0000;
}

uint32_t nullfunc3(void) {
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
    return 0x0000;
}

void nullfunc4(uint32_t c) {
    (void)c;
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
}

const char* nullfunc5(void) {
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
}

uint32_t nullfunc6(uint8_t next_command) {
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
    return 0x0000;
}

uint32_t nullfunc7_no_error(void) {
    return 0;
}

uint32_t nullfunc8_error(uint8_t* config) {
    return 1;
}

void nohelp(void) {
    printf("%s", GET_T(T_MODE_NO_HELP_AVAILABLE));
}

void noperiodic(void) {
    return;
}

void nullfunc1_temp(struct _bytecode* result, struct _bytecode* next) {
    printf("%s\r\n", GET_T(T_MODE_ERROR_NO_EFFECT));
    system_config.error = 1;
}

// all modes and their interaction is handled here
// pirate.h has the conditional defines for modes
struct _mode modes[] = {
    {
        .protocol_name = "HiZ",                // friendly name (promptname)
        .protocol_start = nullfunc1_temp,      // start
        .protocol_start_alt = nullfunc1_temp,  // start with read
        .protocol_stop = nullfunc1_temp,       // stop
        .protocol_stop_alt = nullfunc1_temp,   // stop with read
        .protocol_write = nullfunc1_temp,      // send(/read) max 32 bit
        .protocol_read = nullfunc1_temp,       // read max 32 bit
        .protocol_clkh = nullfunc1_temp,       // set clk high
        .protocol_clkl = nullfunc1_temp,       // set clk low
        .protocol_dath = nullfunc1_temp,       // set dat hi
        .protocol_datl = nullfunc1_temp,       // set dat lo
        .protocol_dats = nullfunc1_temp,       // toggle dat (maybe remove?)
        .protocol_tick_clock = nullfunc1_temp, // tick clk
        .protocol_bitr = nullfunc1_temp,       // read dat pin
        .protocol_periodic =
            noperiodic, // service to regular poll whether a byte has arrived or something interesting has happened
        .protocol_macro = nullfunc4,                     // macro
        .protocol_setup = hiz_setup,                     // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hiz_setup_exec,            // real setup
        .protocol_cleanup = hiz_cleanup,                 // cleanup for HiZ
        //.protocol_settings = hiz_settings,               // display settings
        .protocol_help = hiz_help,                       // display protocol specific help
        .mode_commands = hiz_commands,                   // mode specific commands //ignored if 0x00
        .mode_commands_count = &hiz_commands_count,      // mode specific commands count ignored if 0x00
        .protocol_get_speed = nullfunc7_no_error,        // get the current speed setting of the protocol
        .protocol_command = NULL,                        // per mode command parser - ignored if 0
        .protocol_preflight_sanity_check = NULL,         // sanity check before executing syntax
    },
#ifdef BP_USE_HW1WIRE
    {
        .protocol_name = "1-WIRE",             // friendly name (promptname)
        .protocol_start = hw1wire_start,       // start
        .protocol_start_alt = hw1wire_start,   // start with read
        .protocol_stop = nullfunc1_temp,       // stop
        .protocol_stop_alt = nullfunc1_temp,   // stop with read
        .protocol_write = hw1wire_write,       // send(/read) max 32 bit
        .protocol_read = hw1wire_read,         // read max 32 bit
        .protocol_clkh = nullfunc1_temp,       // set clk high
        .protocol_clkl = nullfunc1_temp,       // set clk low
        .protocol_dath = nullfunc1_temp,       // set dat hi
        .protocol_datl = nullfunc1_temp,       // set dat lo
        .protocol_dats = nullfunc1_temp,       // toggle dat (maybe remove?)
        .protocol_tick_clock = nullfunc1_temp, // tick clk
        .protocol_bitr = nullfunc1_temp,       // read dat pin
        .protocol_periodic =
            noperiodic, // service to regular poll whether a byte has arrived or something interesting has happened
        .protocol_macro = hw1wire_macro,                 // macro
        .protocol_setup = hw1wire_setup,                 // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hw1wire_setup_exc,         // real setup
        .protocol_cleanup = hw1wire_cleanup,             // cleanup for HiZ
        //.protocol_settings = hiz_settings,               // display settings
        .protocol_help = &hw1wire_help,                  // display protocol specific help
        .mode_commands = hw1wire_commands,               // mode specific commands //ignored if 0x00
        .mode_commands_count = &hw1wire_commands_count,  // mode specific commands count ignored if 0x00
        .protocol_get_speed = hw1wire_get_speed,         // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = hw1wire_preflight_sanity_check, // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_HWUART
    {
        .protocol_name = "UART",                         // friendly name (promptname)
        .protocol_start = hwuart_open,                   // start
        .protocol_start_alt = hwuart_open_read,          // start with read
        .protocol_stop = hwuart_close,                   // stop
        .protocol_stop_alt = hwuart_close,               // stop with read
        .protocol_write = hwuart_write,                  // send(/read) max 32 bit
        .protocol_read = hwuart_read,                    // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = hwuart_periodic,            // service to regular poll whether a byte ahs arrived
        .protocol_macro = hwuart_macro,                  // macro
        .protocol_setup = hwuart_setup,                  // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hwuart_setup_exc,          // real setup
        .protocol_cleanup = hwuart_cleanup,              // cleanup for HiZ
        //.protocol_pins=hwuart_pins,				// display pin config
        .protocol_settings = hwuart_settings,          // display settings
        .protocol_help = hwuart_help,                  // display small help about the protocol
        .mode_commands = hwuart_commands,              // mode specific commands
        .mode_commands_count = &hwuart_commands_count, // mode specific commands count
        .protocol_get_speed = hwuart_get_speed,        // get the current speed setting of the protocol
        .protocol_wait_done = hwuart_wait_done,        // wait for the protocol to finish
        .protocol_preflight_sanity_check=hwuart_preflight_sanity_check, // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_HWHDUART
    {
        .protocol_name = "HDUART",                       // friendly name (promptname)
        .protocol_start = hwhduart_open,                 // start
        .protocol_start_alt = hwhduart_start_alt,        // start with read
        .protocol_stop = hwhduart_close,                 // stop
        .protocol_stop_alt = hwhduart_stop_alt,          // stop with read
        .protocol_write = hwhduart_write,                // send(/read) max 32 bit
        .protocol_read = hwhduart_read,                  // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = hwhduart_periodic,          // service to regular poll whether a byte ahs arrived
        .protocol_macro = hwhduart_macro,                // macro
        .protocol_setup = hwhduart_setup,                // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hwhduart_setup_exc,        // real setup
        .protocol_cleanup = hwhduart_cleanup,            // cleanup for HiZ
        //.protocol_pins=hwuart_pins,				// display pin config
        .protocol_settings = hwhduart_settings,          // display settings
        .protocol_help = hwhduart_help,                  // display small help about the protocol
        .mode_commands = hwhduart_commands,              // mode specific commands
        .mode_commands_count = &hwhduart_commands_count, // mode specific commands count
        .protocol_get_speed = hwhduart_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = hwhduart_preflight_sanity_check, // sanity check before executing syntax

    },
#endif
#ifdef BP_USE_HWI2C
    {
        .protocol_name = "I2C",                          // friendly name (promptname)
        .protocol_start = hwi2c_start,                   // start
        .protocol_start_alt = hwi2c_start,               // start with read
        .protocol_stop = hwi2c_stop,                     // stop
        .protocol_stop_alt = hwi2c_stop,                 // stop with read
        .protocol_write = hwi2c_write,                   // send(/read) max 32 bit
        .protocol_read = hwi2c_read,                     // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = hwi2c_macro,                   // macro
        .protocol_setup = hwi2c_setup,                   // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hwi2c_setup_exc,           // real setup
        .protocol_cleanup = hwi2c_cleanup,               // cleanup for HiZ
        //.protocol_pins=HWI2C_pins,				// display pin config
        .protocol_settings = hwi2c_settings,          // display settings
        .protocol_help = hwi2c_help,                  // display small help about the protocol
        .mode_commands = hwi2c_commands,              // mode specific commands
        .mode_commands_count = &hwi2c_commands_count, // mode specific commands count
        .protocol_get_speed = hwi2c_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = hwi2c_preflight_sanity_check, // sanity check before executing syntax

    },
#endif
#ifdef BP_USE_HWSPI
    {
        .protocol_name = "SPI",                // friendly name (promptname)
        .protocol_start = spi_start,           // start
        .protocol_start_alt = spi_startr,      // start with read
        .protocol_stop = spi_stop,             // stop
        .protocol_stop_alt = spi_stopr,        // stop with read
        .protocol_write = spi_write,           // send(/read) max 32 bit
        .protocol_read = spi_read,             // read max 32 bit
        .protocol_clkh = nullfunc1_temp,       // set clk high
        .protocol_clkl = nullfunc1_temp,       // set clk low
        .protocol_dath = nullfunc1_temp,       // set dat hi
        .protocol_datl = nullfunc1_temp,       // set dat lo
        .protocol_dats = nullfunc1_temp,       // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp, // tick clk
        .protocol_bitr = nullfunc1_temp,       // read dat
        .protocol_periodic = noperiodic,       // service to regular poll whether a byte ahs arrived
        .protocol_macro = spi_macro,           // macro
        .protocol_setup = spi_setup,           // setup UI
        .binmode_get_config_length = spi_binmode_get_config_length, // get binmode config length
        .binmode_setup = spi_binmode_setup,                         // setup for binmode
        .protocol_setup_exc = spi_setup_exc,                        // real setup
        .protocol_cleanup = spi_cleanup,                            // cleanup for HiZ
        //.protocol_pins=spi_pins,				// display pin config
        .protocol_settings = spi_settings,            // display settings
        .protocol_help = spi_help,                    // display small help about the protocol
        .mode_commands = hwspi_commands,              // mode specific commands
        .mode_commands_count = &hwspi_commands_count, // mode specific commands count
        .protocol_get_speed = spi_get_speed,          // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = spi_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_HW2WIRE
    {
        .protocol_name = "2WIRE",                        // friendly name (promptname)
        .protocol_start = hw2wire_start,                 // start
        .protocol_start_alt = hw2wire_start_alt,         // start with read
        .protocol_stop = hw2wire_stop,                   // stop
        .protocol_stop_alt = hw2wire_stop_alt,           // stop with read
        .protocol_write = hw2wire_write,                 // send(/read) max 32 bit
        .protocol_read = hw2wire_read,                   // read max 32 bit
        .protocol_clkh = hw2wire_set_clk_high,           // set clk high
        .protocol_clkl = hw2wire_set_clk_low,            // set clk low
        .protocol_dath = hw2wire_set_dat_high,           // set dat hi
        .protocol_datl = hw2wire_set_dat_low,            // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = hw2wire_tick_clock,       // tick clk
        .protocol_bitr = hw2wire_read_bit,               // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = hw2wire_macro,                 // macro
        .protocol_setup = hw2wire_setup,                 // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hw2wire_setup_exc,         // real setup
        .protocol_cleanup = hw2wire_cleanup,             // cleanup for HiZ
        //.protocol_pins=HWI2C_pins,				// display pin config
        .protocol_settings = hw2wire_settings,          // display settings
        .protocol_help = hw2wire_help,                  // display small help about the protocol
        .mode_commands = hw2wire_commands,              // mode specific commands
        .mode_commands_count = &hw2wire_commands_count, // mode specific commands count
        .protocol_get_speed = hw2wire_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = hw2wire_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_HW3WIRE
    {
        .protocol_name = "3WIRE",                        // friendly name (promptname)
        .protocol_start = hw3wire_start,                 // start
        .protocol_start_alt = hw3wire_start_alt,         // start with read
        .protocol_stop = hw3wire_stop,                   // stop
        .protocol_stop_alt = hw3wire_stop_alt,           // stop with read
        .protocol_write = hw3wire_write,                 // send(/read) max 32 bit
        .protocol_read = hw3wire_read,                   // read max 32 bit
        .protocol_clkh = hw3wire_set_clk_high,           // set clk high
        .protocol_clkl = hw3wire_set_clk_low,            // set clk low
        .protocol_dath = hw3wire_set_dat_high,           // set dat hi
        .protocol_datl = hw3wire_set_dat_low,            // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = hw3wire_tick_clock,       // tick clk
        .protocol_bitr = hw3wire_read_bit,               // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = hw3wire_macro,                 // macro
        .protocol_setup = hw3wire_setup,                 // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = hw3wire_setup_exc,         // real setup
        .protocol_cleanup = hw3wire_cleanup,             // cleanup for HiZ
        //.protocol_pins=HWI2C_pins,				// display pin config
        .protocol_settings = hw3wire_settings,          // display settings
        .protocol_help = hw3wire_help,                  // display small help about the protocol
        .mode_commands = hw3wire_commands,              // mode specific commands
        .mode_commands_count = &hw3wire_commands_count, // mode specific commands count
        .protocol_get_speed = hw3wire_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = hw3wire_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_DIO
    {
        .protocol_name = "DIO",                          // friendly name (promptname)
        .protocol_start = nullfunc1_temp,                // start
        .protocol_start_alt = nullfunc1_temp,            // start with read
        .protocol_stop = nullfunc1_temp,                 // stop
        .protocol_stop_alt = nullfunc1_temp,             // stop with read
        .protocol_write = dio_write,                     // send(/read) max 32 bit
        .protocol_read = dio_read,                       // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = dio_macro,                     // macro
        .protocol_setup = dio_setup,                     // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = dio_setup_exc,             // real setup
        .protocol_cleanup = dio_cleanup,                 // cleanup for HiZ
        //.protocol_pins=dio_pins,				// display pin config
        .protocol_settings = dio_settings,          // display settings
        .protocol_help = dio_help,                  // display small help about the protocol
        .mode_commands = dio_commands,              // mode specific commands
        .mode_commands_count = &dio_commands_count, // mode specific commands count
        .protocol_get_speed = dio_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = dio_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_HWLED
    {
        .protocol_name = "LED",                // friendly name (promptname)
        .protocol_start = hwled_start,         // start
        .protocol_start_alt = hwled_start,     // start with read
        .protocol_stop = hwled_stop,           // stop
        .protocol_stop_alt = hwled_stop,       // stop with read
        .protocol_write = hwled_write,         // send(/read) max 32 bit
        .protocol_read = nullfunc1_temp,       // read max 32 bit
        .protocol_clkh = nullfunc1_temp,       // set clk high
        .protocol_clkl = nullfunc1_temp,       // set clk low
        .protocol_dath = nullfunc1_temp,       // set dat hi
        .protocol_datl = nullfunc1_temp,       // set dat lo
        .protocol_dats = nullfunc1_temp,       // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp, // tick clk
        .protocol_bitr = nullfunc1_temp,       // read dat pin
        .protocol_periodic =
            noperiodic, // service to regular poll whether a byte has arrived or something interesting has happened
        .protocol_macro = hwled_macro,                   // macro
        .protocol_setup = hwled_setup,                   // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // binmode get length of config data
        .binmode_setup = nullfunc8_error,                // binmode setup
        .protocol_setup_exc = hwled_setup_exc,           // real setup
        .protocol_cleanup = hwled_cleanup,               // cleanup for HiZ
        //.protocol_pins=HWLED_pins,				// display pin config
        .protocol_settings = hwled_settings,          // display settings
        .protocol_help = hwled_help,                  // display small help about the protocol
        .mode_commands = hwled_commands,              // mode specific commands
        .mode_commands_count = &hwled_commands_count, // mode specific commands count
        .protocol_get_speed = hwled_get_speed,        // get the current speed setting of the protocol
        .protocol_command = NULL,                     // per mode command parser - ignored if 0
        .protocol_wait_done = hwled_wait_idle,        // wait for the protocol to finish
        .protocol_preflight_sanity_check = hwled_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_INFRARED
    {
        .protocol_name = "INFRARED",                     // friendly name (promptname)
        .protocol_start = nullfunc1_temp,                // start
        .protocol_start_alt = nullfunc1_temp,            // start with read
        .protocol_stop = nullfunc1_temp,                 // stop
        .protocol_stop_alt = nullfunc1_temp,             // stop with read
        .protocol_write = infrared_write,                // send(/read) max 32 bit
        .protocol_read = infrared_read,                  // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = infrared_periodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = infrared_macro,                // macro
        .protocol_setup = infrared_setup,                // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // binmode get length of config data
        .binmode_setup = nullfunc8_error,                // binmode setup
        .protocol_setup_exc = infrared_setup_exc,        // real setup
        .protocol_cleanup = infrared_cleanup,            // cleanup for HiZ
        //.protocol_pins=infrared_pins,				// display pin config
        .protocol_settings = infrared_settings,          // display settings
        .protocol_help = infrared_help,                  // display small help about the protocol
        .mode_commands = infrared_commands,              // mode specific commands
        .mode_commands_count = &infrared_commands_count, // mode specific commands count
        .protocol_wait_done = infrared_wait_idle,        // wait for the protocol to finish
        .protocol_get_speed = infrared_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = infrared_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_JTAG
    {
        .protocol_name = "JTAG",                          // friendly name (promptname)
        .protocol_start = nullfunc1_temp,                // start
        .protocol_start_alt = nullfunc1_temp,            // start with read
        .protocol_stop = nullfunc1_temp,                 // stop
        .protocol_stop_alt = nullfunc1_temp,             // stop with read
        .protocol_write = nullfunc1_temp,                     // send(/read) max 32 bit
        .protocol_read = nullfunc1_temp,                       // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = nullfunc4,                     // macro
        .protocol_setup = jtag_setup,                     // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = jtag_setup_exc,             // real setup
        .protocol_cleanup = jtag_cleanup,                 // cleanup for HiZ
        //.protocol_pins=dio_pins,				// display pin config
        .protocol_settings = jtag_settings,          // display settings
        .protocol_help = jtag_help,                  // display small help about the protocol
        .mode_commands = jtag_commands,              // mode specific commands
        .mode_commands_count = &jtag_commands_count, // mode specific commands count
        .protocol_get_speed = jtag_get_speed,        // get the current speed setting of the protocol
        .protocol_preflight_sanity_check = jtag_preflight_sanity_check,      // sanity check before executing syntax
    },
#endif
#ifdef BP_USE_DUMMY1
    {
        .protocol_name = "DUMMY1",                       // friendly name (promptname)
        .protocol_start = dummy1_start,                  // start
        .protocol_start_alt = dummy1_start,              // start with read
        .protocol_stop = dummy1_stop,                    // stop
        .protocol_stop_alt = dummy1_stop,                // stop with read
        .protocol_write = dummy1_write,                  // send(/read) max 32 bit
        .protocol_read = dummy1_read,                    // read max 32 bit
        .protocol_clkh = dummy1_clkh,                    // set clk high
        .protocol_clkl = dummy1_clkl,                    // set clk low
        .protocol_dath = dummy1_dath,                    // set dat hi
        .protocol_datl = dummy1_datl,                    // set dat lo
        .protocol_dats = dummy1_dats,                    // toggle dat (?)
        .protocol_tick_clock = dummy1_clk,               // toggle clk (?)
        .protocol_bitr = dummy1_bitr,                    // read 1 bit (?)
        .protocol_periodic = dummy1_periodic,            // service to regular poll whether a byte ahs arrived
        .protocol_macro = dummy1_macro,                  // macro
        .protocol_setup = dummy1_setup,                  // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // binmode get length of config data
        .binmode_setup = nullfunc8_error,                // binmode setup
        .protocol_setup_exc = dummy1_setup_exc,          // real setup
        .protocol_cleanup = dummy1_cleanup,              // cleanup for HiZ
        //.protocol_pins=dummy1_pins,				// display pin config
        .protocol_settings = dummy1_settings,          // display settings
        .protocol_help = dummy1_help,                  // display small help about the protocol
        .mode_commands = dummy1_commands,              // mode specific commands
        .mode_commands_count = &dummy1_commands_count, // mode specific commands count
        .protocol_get_speed = nullfunc7_no_error,      // get the current speed setting of the protocol
    },
#endif
#ifdef BP_USE_BINLOOPBACK
    {
        .protocol_name = "BIN"                                   // friendly name (promptname)
        .protocol_start = binloopback_open, // start
        .protocol_start_alt = binloopback_open_read,             // start with read
        .protocol_stop = binloopback_close,                      // stop
        .protocol_stop_alt = binloopback_close,                  // stop with read
        .protocol_write = binloopback_write,                     // send(/read) max 32 bit
        .protocol_read = nullfunc1_temp,                         // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                         // set clk high
        .protocol_clkl = nullfunc1_temp,                         // set clk low
        .protocol_dath = nullfunc1_temp,                         // set dat hi
        .protocol_datl = nullfunc1_temp,                         // set dat lo
        .protocol_dats = nullfunc1_temp,                         // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,                   // tick clk
        .protocol_bitr = nullfunc1_temp,                         // read dat
        .protocol_periodic = binloopback_periodic,               // service to regular poll whether a byte ahs arrived
        .protocol_macro = hwled_macro,                           // macro
        .protocol_setup = binloopback_setup,                     // setup UI
        .binmode_get_config_length = nullfunc7_no_error,         // binmode get length of config data
        .binmode_setup = nullfunc8_error,                        // binmode setup
        .protocol_setup_exc = binloopback_setup_exc,             // real setup
        .protocol_cleanup = binloopback_cleanup,                 // cleanup for HiZ
        //.protocol_pins=HWLED_pins,				// display pin config
        .protocol_settings = hwled_settings,                // display settings
        .protocol_help = hwled_help,                        // display small help about the protocol
        .mode_commands = binloopback_commands,              // mode specific commands
        .mode_commands_count = &binloopback_commands_count, // mode specific commands count
        .protocol_get_speed = nullfunc7_no_error,           // get the current speed setting of the protocol
    },
#endif
#ifdef BP_USE_LCDSPI
    {
        .protocol_name = "LCDSPI",                       // friendly name (promptname)
        .protocol_start = nullfunc1,                     // start
        .protocol_start_alt = nullfunc1,                 // start with read
        .protocol_stop = nullfunc1,                      // stop
        .protocol_stop_alt = nullfunc1,                  // stop with read
        .protocol_write = LCDSPI_send,                   // send(/read) max 32 bit
        .protocol_read = LCDSPI_read,                    // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = LCDSPI_macro,                  // macro
        .protocol_setup = LCDSPI_setup,                  // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = LCDSPI_setup_exc,          // real setup
        .protocol_cleanup = LCDSPI_cleanup,              // cleanup for HiZ
        //.protocol_pins=LCDSPI_pins,				// display pin config
        .protocol_settings = LCDSPI_settings,     // display settings
        .protocol_help = nohelp,                  // display small help about the protocol
        .mode_commands = NULL,                    // mode specific commands
        .mode_commands_count = NULL,              // mode specific commands count
        .protocol_get_speed = nullfunc7_no_error, // get the current speed setting of the protocol
    },
#endif
#ifdef BP_USE_LCDI2C
    {
        .protocol_name = "LCDI2C",                       // friendly name (promptname)
        .protocol_start = nullfunc1,                     // start
        .protocol_start_alt = nullfunc1,                 // start with read
        .protocol_stop = nullfunc1,                      // stop
        .protocol_stop_alt = nullfunc1,                  // stop with read
        .protocol_write = LCDI2C_send,                   // send(/read) max 32 bit
        .protocol_read = LCDI2C_read,                    // read max 32 bit
        .protocol_clkh = nullfunc1_temp,                 // set clk high
        .protocol_clkl = nullfunc1_temp,                 // set clk low
        .protocol_dath = nullfunc1_temp,                 // set dat hi
        .protocol_datl = nullfunc1_temp,                 // set dat lo
        .protocol_dats = nullfunc1_temp,                 // toggle dat (remove?)
        .protocol_tick_clock = nullfunc1_temp,           // tick clk
        .protocol_bitr = nullfunc1_temp,                 // read dat
        .protocol_periodic = noperiodic,                 // service to regular poll whether a byte ahs arrived
        .protocol_macro = LCDI2C_macro,                  // macro
        .protocol_setup = LCDI2C_setup,                  // setup UI
        .binmode_get_config_length = nullfunc7_no_error, // get binmode config length
        .binmode_setup = nullfunc8_error,                // setup for binmode
        .protocol_setup_exc = LCDI2C_setup_exc,          // real setup
        .protocol_cleanup = LCDI2C_cleanup,              // cleanup for HiZ
        //.protocol_pins=LCDI2C_pins,				// display pin config
        .protocol_settings = LCDI2C_settings,     // display settings
        .protocol_help = nohelp,                  // display small help about the protocol
        .mode_commands = NULL,                    // mode specific commands
        .mode_commands_count = 0,                 // mode specific commands count
        .protocol_get_speed = nullfunc7_no_error, // get the current speed setting of the protocol
    },
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
