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
    static uint8_t static_message_count = 0u;

    // Basic concepts:
    // 1. Encode a counter within the message ID.
    //    This prevents various code errors from
    //    causing intercore synchronization to be lost.
    //    The counter is incremented each time core0
    //    sends a message using this API, and the wait
    //    ensures this same message (including the counter)
    //    is received, confirming the message was processed.
    // 2. Ensure that the message ID is not a valid pointer.
    //    Per the RP2040 memory map, the range 0x60000000..0xCFFFFFFF
    //    is unmapped, and will cause a bus error if accessed.
    //
    // RP2040 uses a `uint32_t` value for the intercore message.
    //
    // Here, we pack that `uint32_t` with the following data:
    // typedef struct {
    //     uint8_t map          = 0xABu;      // constant
    //     uint8_t cnt          = counter;    // incremented each time a message is sent
    //     uint8_t RFU          = 0x00u;      // reserved for future use
    //     bp_icm_message_t msg = message_id; // actual message
    // } bp_icm_raw_message_t;

    bp_icm_raw_message_t raw_msg = {
        .map = 0x80u,
        .cnt = ++static_message_count,
        .rfu = 0x00u,
        .msg = message_id
    };
    multicore_fifo_push_blocking(raw_msg.raw);

    do {
        uint32_t response = multicore_fifo_pop_blocking();
        // in the current design, any message other than
        // the one sent indicates a serious de-synchronization
        assert(response == raw_msg.raw);
        if (response == raw_msg.raw) return;
    } while (1);
}
