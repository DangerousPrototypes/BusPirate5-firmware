#pragma once

#include "pirate.h"
#include "lib/rtt/RTT/SEGGER_RTT.h"

// To add a new "category" of debug messages which can be enabled/disabled
// at runtime separately from existing categories, there are two steps:
// 1. Add a new constant to the `_bp_debug_category_enum_t` enumeration.
// 2. Add a new `#define BP_DEBUG_CAT_...` constant to the list below.
//
// NOTE: New categories are not needed for temporary use.
//       Use the `BP_DEBUG_CAT_TEMP` category for those,
//       or just use the default category (`BP_DEBUG_CAT_CATCHALL`).


// C11 doesn't have typesafe enums, but the following tomfoolery can partially
// emulates typesafe enums, with a minor one-time coding cost when adding
// a new enum value.
#if 1

    // define the enumeration here, but not as a type that would naturally be used in code
    // outside the debug header.
    // NOTE: intentionally non-consecutive values.
    typedef enum { 
        E_DEBUG_LEVEL_FATAL   = 0x00u,
        E_DEBUG_LEVEL_ERROR   = 0x20u,
        E_DEBUG_LEVEL_WARNING = 0x60u,
        E_DEBUG_LEVEL_INFO    = 0xA0u,
        E_DEBUG_LEVEL_VERBOSE = 0xC0u,
        E_DEBUG_LEVEL_DEBUG   = 0xE0u,
        E_DEBUG_LEVEL_NEVER   = 0xFFu,
    } _bp_debug_level_enum_t;

    // NOTE: Maximum value for this enum is *** 32u ***.  See comment following the definition.
    typedef enum _bp_debug_category_enum_t {
        E_DEBUG_CAT_CATCHALL         =  0u, // (((uint32_t)1u) <<  0u), // for messages that are not (yet) categorized
        E_DEBUG_CAT_EARLY_BOOT       =  1u, // (((uint32_t)1u) <<  1u), // early-in-boot (initialization)
        // E_DEBUG_CAT_ONBOARD_PIXELS   = XXu, // (((uint32_t)1u) << XXu), // onboard RGB pixels
        // E_DEBUG_CAT_USB_HID          = XXu, // (((uint32_t)1u) << XXu), // USB based HID interactions
        // E_DEBUG_CAT_USB_CDC          = XXu, // (((uint32_t)1u) << XXu), // USB based serial port
        // E_DEBUG_CAT_USB_MSC          = XXu, // (((uint32_t)1u) << XXu), // USB based mass storage commands
        // E_DEBUG_CAT_FLASH_MEDIA      = XXu, // (((uint32_t)1u) << XXu), // flash media operations
        // E_DEBUG_CAT_UI               = XXu, // (((uint32_t)1u) << XXu), // for files in `src/ui/...`
        // E_DEBUG_CAT_BINMODE_FALA     = XXu, // (((uint32_t)1u) << XXu), // follow-along logic analyzer
        // E_DEBUG_CAT_BINMODE_SUMP     = XXu, // (((uint32_t)1u) << XXu), // SUMP mode (logic analyzer)
        // E_DEBUG_CAT_BYTECODE         = XXu, // (((uint32_t)1u) << XXu), // aka syntax parsing into bytecode, bytecode execution
        // E_DEBUG_CAT_MODE_GLOBAL      = XXu, // (((uint32_t)1u) << XXu), // global mode parsing
        // E_DEBUG_CAT_MODE_1WIRE       = XXu, // (((uint32_t)1u) << XXu), // 1-wire mode specific
        // E_DEBUG_CAT_MODE_2WIRE       = XXu, // (((uint32_t)1u) << XXu), // 2-wire mode specific
        // E_DEBUG_CAT_MODE_UART        = XXu, // (((uint32_t)1u) << XXu), // uart mode specific
        // E_DEBUG_CAT_MODE_I2C         = XXu, // (((uint32_t)1u) << XXu), // i2c mode specific
        // E_DEBUG_CAT_MODE_SPI         = XXu, // (((uint32_t)1u) << XXu), // spi mode specific
        // E_DEBUG_CAT_MODE_DIO         = XXu, // (((uint32_t)1u) << XXu), // dio mode specific
        // E_DEBUG_CAT_MODE_HWLED       = XXu, // (((uint32_t)1u) << XXu), // hwled mode specific
        // E_DEBUG_CAT_MODE_INFRARED    = XXu, // (((uint32_t)1u) << XXu), // infrared mode specific
        // E_DEBUG_CAT_MODE_BINLOOPBACK = XXu, // (((uint32_t)1u) << XXu), // binloopback mode specific
        // E_DEBUG_CAT_MODE_LCDSPI      = XXu, // (((uint32_t)1u) << XXu), // lcdspi mode specific
        // E_DEBUG_CAT_MODE_LCDI2C      = XXu, // (((uint32_t)1u) << XXu), // lcdi2c mode specific
        E_DEBUG_CAT_TEMP             = 31u, // (((uint32_t)1u) << 31u), // use for temporary debug messages, also the maximum index
    } _bp_debug_category_enum_t;
    // NOTE: Maximum value for this enum is *** 31u ***.
    //       This enum is used two ways:
    //       1. as an index into an array of debug levels
    //       2. to set a single bit within a `uint32_t`, which is usable to enable/disable the category's output.
    //       Any value greater than 31u will cause the bitmask to be zero, and that category's output will never appear.

    // next, define a structure that wraps the enumeration.
    // the structure's member will have the enum's type.
    typedef struct _bp_debug_level_t    { _bp_debug_level_enum_t    level;    } bp_debug_level_t;
    typedef struct _bp_debug_category_t { _bp_debug_category_enum_t category; } bp_debug_category_t;

    _Static_assert(sizeof(bp_debug_level_t)    == sizeof(uint8_t), "bp_debug_level_t    is not the expected size");
    _Static_assert(sizeof(bp_debug_category_t) == sizeof(uint8_t), "bp_debug_category_t is not the expected size");

#endif

// Define the user-facing constants using the wrapper structure.
// Thus, if a function is defined to take a `struct bp_debug_level`, but
// is passed an incorrect enum type (e.g., swapping LEVEL and CATEGORY):
// 1. the compiler will emit an error (incompatible argument type)
// 2. the generated code will have ZERO overhead vs. a straight enum
// While this is a small amount of overhead while coding, it seems
// worthwhile to take whatever type safety we can wring out of C.

#define BP_DEBUG_LEVEL_FATAL          ((bp_debug_level_t)   { E_DEBUG_LEVEL_FATAL          }) // unrecoerable errors that normally crash or require reset
#define BP_DEBUG_LEVEL_ERROR          ((bp_debug_level_t)   { E_DEBUG_LEVEL_ERROR          }) // recoverable errors. 
#define BP_DEBUG_LEVEL_WARNING        ((bp_debug_level_t)   { E_DEBUG_LEVEL_WARNING        }) // warnings, such as improper user input, edge cases occurring, potential (unveritifer) issues
#define BP_DEBUG_LEVEL_INFO           ((bp_debug_level_t)   { E_DEBUG_LEVEL_INFO           }) // informational messages
#define BP_DEBUG_LEVEL_VERBOSE        ((bp_debug_level_t)   { E_DEBUG_LEVEL_VERBOSE        }) 
#define BP_DEBUG_LEVEL_DEBUG          ((bp_debug_level_t)   { E_DEBUG_LEVEL_DEBUG          }) // 
#define BP_DEBUG_LEVEL_NEVER          ((bp_debug_level_t)   { E_DEBUG_LEVEL_NEVER          })

#define BP_DEBUG_CAT_CATCHALL         ((bp_debug_category_t){ E_DEBUG_CAT_CATCHALL         }) // for messages that are not (yet) categorized
#define BP_DEBUG_CAT_EARLY_BOOT       ((bp_debug_category_t){ E_DEBUG_CAT_EARLY_BOOT       }) // early-in-boot (initialization)
// #define BP_DEBUG_CAT_ONBOARD_PIXELS   ((bp_debug_category_t){ E_DEBUG_CAT_ONBOARD_PIXELS   }) // onboard RGB pixels
// #define BP_DEBUG_CAT_USB_HID          ((bp_debug_category_t){ E_DEBUG_CAT_USB_HID          }) // USB based HID interactions
// #define BP_DEBUG_CAT_USB_CDC          ((bp_debug_category_t){ E_DEBUG_CAT_USB_CDC          }) // USB based serial port
// #define BP_DEBUG_CAT_USB_MSC          ((bp_debug_category_t){ E_DEBUG_CAT_USB_MSC          }) // USB based mass storage commands
// #define BP_DEBUG_CAT_FLASH_MEDIA      ((bp_debug_category_t){ E_DEBUG_CAT_FLASH_MEDIA      }) // flash media operations
// #define BP_DEBUG_CAT_UI               ((bp_debug_category_t){ E_DEBUG_CAT_UI               }) // for files in `src/ui/...`
// #define BP_DEBUG_CAT_BINMODE_FALA     ((bp_debug_category_t){ E_DEBUG_CAT_BINMODE_FALA     }) // follow-along logic analyzer
// #define BP_DEBUG_CAT_BINMODE_SUMP     ((bp_debug_category_t){ E_DEBUG_CAT_BINMODE_SUMP     }) // SUMP mode (logic analyzer)
// #define BP_DEBUG_CAT_BYTECODE         ((bp_debug_category_t){ E_DEBUG_CAT_BYTECODE         }) // aka syntax parsing into bytecode, bytecode execution
// #define BP_DEBUG_CAT_MODE_GLOBAL      ((bp_debug_category_t){ E_DEBUG_CAT_MODE_GLOBAL      }) // global mode parsing
// #define BP_DEBUG_CAT_MODE_1WIRE       ((bp_debug_category_t){ E_DEBUG_CAT_MODE_1WIRE       }) // 1-wire mode specific
// #define BP_DEBUG_CAT_MODE_2WIRE       ((bp_debug_category_t){ E_DEBUG_CAT_MODE_2WIRE       }) // 2-wire mode specific
// #define BP_DEBUG_CAT_MODE_UART        ((bp_debug_category_t){ E_DEBUG_CAT_MODE_UART        }) // uart mode specific
// #define BP_DEBUG_CAT_MODE_I2C         ((bp_debug_category_t){ E_DEBUG_CAT_MODE_I2C         }) // i2c mode specific
// #define BP_DEBUG_CAT_MODE_SPI         ((bp_debug_category_t){ E_DEBUG_CAT_MODE_SPI         }) // spi mode specific
// #define BP_DEBUG_CAT_MODE_DIO         ((bp_debug_category_t){ E_DEBUG_CAT_MODE_DIO         }) // dio mode specific
// #define BP_DEBUG_CAT_MODE_HWLED       ((bp_debug_category_t){ E_DEBUG_CAT_MODE_HWLED       }) // hwled mode specific
// #define BP_DEBUG_CAT_MODE_INFRARED    ((bp_debug_category_t){ E_DEBUG_CAT_MODE_INFRARED    }) // infrared mode specific
// #define BP_DEBUG_CAT_MODE_BINLOOPBACK ((bp_debug_category_t){ E_DEBUG_CAT_MODE_BINLOOPBACK }) // binloopback mode specific
// #define BP_DEBUG_CAT_MODE_LCDSPI      ((bp_debug_category_t){ E_DEBUG_CAT_MODE_LCDSPI      }) // lcdspi mode specific
// #define BP_DEBUG_CAT_MODE_LCDI2C      ((bp_debug_category_t){ E_DEBUG_CAT_MODE_LCDI2C      }) // lcdi2c mode specific
// ... reserved for future use ...
#define BP_DEBUG_CAT_TEMP             ((bp_debug_category_t){ E_DEBUG_CAT_TEMP             }) // use for temporary debug messages



extern uint32_t         _DEBUG_ENABLED_CATEGORIES; // mask of enabled categories
extern bp_debug_level_t _DEBUG_LEVELS[32]; // up to 32 categories, each with a debug level


#define DEBUG_CATEGORY_TO_MASK(_CAT) \
    ( (uint32_t) (((uint32_t)1u) << (uint32_t)(_CAT).category) )


// This is the underlying debug macro logic, also used by the other debug macros.
#define BP_DEBUG_PRINT(_LEVEL, _CATEGORY, ...) \
    do {                                                                                    \
        const bp_debug_level_t    level    = (_LEVEL);                                            \
        const bp_debug_category_t category = (_CATEGORY);                                         \
        bool output = false;                                                                \
        if (level.level == (BP_DEBUG_LEVEL_FATAL).level) {                                  \
            output = true;                                                                  \
        } else                                                                              \
        if (level.level <= _DEBUG_LEVELS[0].level) {                                        \
            output = true;                                                                  \
        } else                                                                              \
        if ((level.level <= _DEBUG_LEVELS[category.category].level) &&                      \
            ((_DEBUG_ENABLED_CATEGORIES & DEBUG_CATEGORY_TO_MASK(category)) != 0)) {        \
            output = true;                                                                  \
        }                                                                                   \
        if (output) {                                                                       \
            SEGGER_RTT_printf(0, __VA_ARGS__);                                              \
        }                                                                                   \
    } while (0);



 #define _BP_STRINGIFY( L )  #L 
 #define MakeString( L ) _BP_STRINGIFY( L )

// USAGE:
// * Files can immediately use the PRINT_* macros.  This will use the `CATCHALL` category.
// * Files can also immediately use the BP_DEBUG_PRINT() macro directly, although it is
//   slightly more typing.
// * Where a file is exclusively (or primarily) a single category, the source file may
//   define the `BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY` token prior to including headers.
//   For example, for the PIXEL category:
//
//   #define BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY BP_DEBUG_CAT_PIXELS
//
//   Doing so will change all use of the PRINT_* macros to use the PIXEL category for
//   that compilation unit (e.g., source C file).  The BP_DEBUG_PRINT() macro can still
//   be used to specify another category, if needed.
//
#if defined(BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY)
    // #pragma message( "Override cateory for PRINT_* macros: " MakeString(BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY) )
    #define BP_DEBUG_DEFAULT_CATEGORY BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY
#else
    // #pragma message( "Using default catch-all category for PRINT_* macros: " MakeString(BP_DEBUG_CAT_CATCHALL) )
    #define BP_DEBUG_DEFAULT_CATEGORY BP_DEBUG_CAT_CATCHALL
#endif

#if defined(DISABLE_DEBUG_PRINT_MACROS)

#define PRINT_FATAL(...)
#define PRINT_ERROR(...)
#define PRINT_WARNING(...)
#define PRINT_INFO(...)
#define PRINT_VERBOSE(...)
#define PRINT_DEBUG(...)

#else

#define PRINT_FATAL(...)   BP_DEBUG_PRINT(BP_DEBUG_LEVEL_FATAL,   BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#define PRINT_ERROR(...)   BP_DEBUG_PRINT(BP_DEBUG_LEVEL_ERROR,   BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#define PRINT_WARNING(...) BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#define PRINT_INFO(...)    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_INFO,    BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#define PRINT_VERBOSE(...) BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)
#define PRINT_DEBUG(...)   BP_DEBUG_PRINT(BP_DEBUG_LEVEL_DEBUG,   BP_DEBUG_DEFAULT_CATEGORY, __VA_ARGS__)

#endif
