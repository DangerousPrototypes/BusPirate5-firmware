
#include "debug_rtt.h"

uint32_t         _DEBUG_ENABLED_CATEGORIES; // mask of enabled debug categories
bp_debug_level_t _DEBUG_LEVELS[32]; // up to 32 categories, each with a debug level

