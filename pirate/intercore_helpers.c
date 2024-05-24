#include "pirate/intercore_helpers.h"
#include "pirate.h"
#include "assert.h"


// This wrapper is a response to manual code review noting
// at least two coding patterns, neither of which provided
// any indication when things were awry.
//
// Using this wrapper increases the detectability of code
// errors, and also makes it easier to later change to
// alternative cross-core communication methods if needed.


void icm_core0_send_message_synchronous(bp_icm_message_t message_id) {
    // NOTE: although split into multiple lines for readability,
    //       the compiler optimizes this quite well!

    static_assert(sizeof(message_id) <= 2); // else have to update encoding / decoding

    static uint8_t static_message_count = 0u;

    // Basic concepts:
    // 1. Encode a counter within the message ID.
    //    This prevents various code errors from
    //    causing intercore synchronization to be lost.
    //    The counter is incremented each time core0
    //    sends a message using this API, and the wait
    //    ensure this same message is received confirming
    //    the message was processed.
    // 2. Ensure that the message ID is not a valid pointer.
    //    Per the RP2040 memory map, the range 0x60000000..0xCFFFFFFF
    //    is unmapped, and will cause a bus error if accessed.
    //
    // RP2040 uses a `uint32_t` value for the message.
    // Currently this is functionally equivalent to:
    // typedef struct {
    //     uint32_t map   : 8 = 0x80u;      // constant
    //     uint32_t cnt   : 8 = counter;    // incremented each time a message is sent
    //     uint32_t RFU   : 8 = 0x00u;      // reserved for future use
    //     uint32_t Value : 8 = message_id; // actual message
    // } icm_message_t;
    // 

    // Values in range 0x60000000 to 0xCFFFFFFF are unmapped on RP2040
    // Choosing 0x80000000..0x80FFFFFF for intercore messages
    ++static_message_count;
    uint32_t icm_value =
        (uint32_t)0x80000000u
        |  (((uint32_t)static_message_count) << 16)
        |  message_id;
    multicore_fifo_push_blocking(icm_value);

    do {
        uint32_t response = multicore_fifo_pop_blocking();
        // in the current design, any message other than
        // the one sent indicates a serious de-synchronization
        assert(response == message_id);
        if (response == message_id) return;
    } while (1);
}
