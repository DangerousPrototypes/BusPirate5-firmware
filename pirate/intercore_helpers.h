// Inter-Core Messages

#include "pico/multicore.h"

// This file added in response to manual code review discovering the following:
//     Some places, core0 sends, and then loops until core1 sends ***same*** value as response.
//     (This could lose other messages that are sent, if any.)
//
//     Other places, core0 sends, and then loops until core1 sends ***any*** value as response.
//     (This could improperly continue even if the response is for a different message.)
//
//     There are strong warnings AGAINST using multicore FIFOs unless can be sure not used
//     by any RTOS layer, intercore lockout functionality, etc.  Thus, consider changing these
//     to use a standard queue instead.
//
typedef uint8_t bp_icm_message_t;
#define BP_ICM_INIT_CORE1            ((bp_icm_message_t)0xA5) // Used to synchronize Core0 and Core1 intialization.
#define BP_ICM_DISABLE_LCD_UPDATES   ((bp_icm_message_t)0xF0) // disables LCD updates / IRQ
#define BP_ICM_ENABLE_LCD_UPDATES    ((bp_icm_message_t)0xF1) // enables LCD updates / IRQ
#define BP_ICM_FORCE_LCD_UPDATE      ((bp_icm_message_t)0xF2) // enable LCD updates / IRQ and force an update
#define BP_ICM_DISABLE_RGB_UPDATES   ((bp_icm_message_t)0xF3) // disables RGB updates / IRQ
#define BP_ICM_ENABLE_RGB_UPDATES    ((bp_icm_message_t)0xF4) // enables RGB updates / IRQ

// This is intended to be an opaque data type
typedef struct _bp_icm_raw_message_t {
    union {
        uint32_t raw;
        struct {
            uint8_t map;          // set to 0xABu to create invalid pointer, per RP2040 memory map
            uint8_t cnt;          // incremented each time a message is sent, to catch desynchronization
            uint8_t rfu;          // reserved for future use
            bp_icm_message_t msg; // actual message, aka bp_icm_message_t
        };
    };
} bp_icm_raw_message_t; // wrapper to clarify that this differs from bp_icm_message_t
static_assert(sizeof(bp_icm_message_t) == sizeof(uint8_t), "sizeof(bp_icm_message_t) must be sizeof(uint8_t), else structure must be updated");
static_assert(sizeof(bp_icm_raw_message_t) == sizeof(uint32_t), "sizeof(bp_icm_raw_message_t) must be sizeof(uint32_t) to be used by intercore messaging");


// While core1 has a single point where these messages are received / responded to,
// core0 sends the messages from many files.
// Funnel into low-overhead wrapper to catch errors in cross-core synchronization,
// because debugging such issues is ... non-trivial.
void icm_core0_send_message_synchronous(bp_icm_message_t message_id);


// These are ultra-low overhead ... maybe even zero overhead with full optimizations.
inline bp_icm_raw_message_t icm_core1_get_raw_message() {
    bp_icm_raw_message_t result;
    result.raw = multicore_fifo_pop_blocking();
    return result;
}
inline void icm_core1_notify_completion(bp_icm_raw_message_t icm_raw_value) {
    multicore_fifo_push_blocking(icm_raw_value.raw);
}
inline bp_icm_message_t get_embedded_message(bp_icm_raw_message_t icm_raw_value) {
    return icm_raw_value.msg;
}
