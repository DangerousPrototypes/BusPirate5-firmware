// Inter-Core Messages

#include "pico/multicore.h"

// This file added in response to manual code review discovering the following:
//     Some places, core0 sends, and then loops until core1 sends ***same*** value as response.
//     (This could lose other messages that are sent, if any.)
//
//     Other places, core0 sends, and then loops until core1 sends ***any*** value as response.
//     (This could improperly continue even if the response is for a different message.)
//

// Having defined names (rather than magic numbers) improves readability of the code.
#define BP_ICM_INIT_CORE1__REQUEST   ((uint8_t)0x00) // BUGBUG -- Most messages use same ID for request/response...
#define BP_ICM_INIT_CORE1__COMPLETE  ((uint8_t)0xFF) // BUGBUG -- Most messages use same ID for request/response...
#define BP_ICM_DISABLE_LCD_UPDATES   ((uint8_t)0xF0) // disables LCD updates / IRQ
#define BP_ICM_ENABLE_LCD_UPDATES    ((uint8_t)0xF1) // enables LCD updates / IRQ
#define BP_ICM_FORCE_LCD_UPDATE      ((uint8_t)0xF2) // enable LCD updates / IRQ and force an update
#define BP_ICM_DISABLE_RGB_UPDATES   ((uint8_t)0xF3) // disables RGB updates / IRQ
#define BP_ICM_ENABLE_RGB_UPDATES    ((uint8_t)0xF4) // enables RGB updates / IRQ

// While core1 has a single point where these messages are received / responded to,
// core0 sends the messages from many files.
// Funnel into low-overhead wrapper to catch errors in cross-core synchronization,
// because debugging such issues is ... non-trivial.
void icm_core0_send_message(uint8_t message_id);


// Ultra-low overhead ... maybe even zero overhead with full optimizations.
inline uint8_t icm_get_message(uint32_t icm_value) {
    return (uint8_t)(icm_value & 0xFFu);
}
