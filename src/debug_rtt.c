
#include "debug_rtt.h"


// In C, there does not appear to be a way to use the
// typesafe versions of the enum values.  :(
// Therefore, must directly use the E_DEBUG_CAT_... 
// values in this file.  Keep all such use here...
#define _DEBUG_E_CAT_TO_MASK(_E_DEBUG_VALUE) \
    ( (uint32_t) (((uint32_t)1u) << _E_DEBUG_VALUE) )    

uint32_t         _DEBUG_ENABLED_CATEGORIES = // mask of enabled debug categories
    _DEBUG_E_CAT_TO_MASK(E_DEBUG_CAT_CATCHALL)            |
    _DEBUG_E_CAT_TO_MASK(E_DEBUG_CAT_EARLY_BOOT)          |
    // _DEBUG_E_CAT_TO_MASK(E_DEBUG_CAT_ONBOARD_PIXELS)      |
    _DEBUG_E_CAT_TO_MASK(E_DEBUG_CAT_ONBOARD_STORAGE)     |
    _DEBUG_E_CAT_TO_MASK(E_DEBUG_CAT_TEMP)                |
    0u;

// If listed, will be initialized to the specified value.
// Otherwise, will be initialized to zero (BP_DEBUG_LEVEL_FATAL).
// Unless enabled in the bitmask above, only fatal messages
// will be output.
bp_debug_level_t _DEBUG_LEVELS[32] = {
    [E_DEBUG_CAT_CATCHALL       ] = BP_DEBUG_LEVEL_FATAL,
    [E_DEBUG_CAT_EARLY_BOOT     ] = BP_DEBUG_LEVEL_VERBOSE,
    [E_DEBUG_CAT_ONBOARD_PIXELS ] = BP_DEBUG_LEVEL_VERBOSE,
    [E_DEBUG_CAT_ONBOARD_STORAGE] = BP_DEBUG_LEVEL_VERBOSE,
    // add others here, in order of enumeration value
    [E_DEBUG_CAT_TEMP           ] = BP_DEBUG_LEVEL_NEVER, // Print EVERYTHING in TEMP category by default
}; // up to 32 categories, each with a debug level

