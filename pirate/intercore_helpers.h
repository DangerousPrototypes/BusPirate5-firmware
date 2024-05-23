// Inter-Core Messages

#include "pico/multicore.h"

// BUGBUG -- Not using particularly safe techniques...
//           Some places, core0 sends, and then loops until core1 sends same value as response.
//           (This could lose other messages that are sent, if any.)
//           Other places, core0 sends, and then loops until core1 sends any value as response.
//           (This could improperly continue even if the response is for a different message.)
#define BP_ICM_INIT_CORE1__REQUEST   ((uint8_t)0x00) // BUGBUG -- Most messages use same ID for request/response...
#define BP_ICM_INIT_CORE1__COMPLETE  ((uint8_t)0xFF) // BUGBUG -- Most messages use same ID for request/response...
#define BP_ICM_VALUE_F0              ((uint8_t)0xF0) // disables LCD IRQ, disabled LCD updates
#define BP_ICM_VALUE_F1              ((uint8_t)0xF1) // enables LCD IRQ, enables LCD updates
#define BP_ICM_VALUE_F2              ((uint8_t)0xF2) // enable LCD IRQ, enabled and forces LCD update
#define BP_ICM_VALUE_F3              ((uint8_t)0xF3) // disables RGB IRQ
#define BP_ICM_VALUE_F4              ((uint8_t)0xF4) // enables RGB IRQ

void icm_core0_send_message(uint8_t message_id);
inline uint8_t icm_get_message(uint32_t icm_value) {
    return (uint8_t)(icm_value & 0xFFu);
}
